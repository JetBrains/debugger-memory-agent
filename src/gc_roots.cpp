// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <set>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include "gc_roots.h"
#include "utils.h"
#include "log.h"

class referenceInfo {
public:
    referenceInfo(jlong tag, jvmtiHeapReferenceKind kind) : tag_(tag), kind_(kind) {}

    virtual ~referenceInfo() = default;

    virtual jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) {
        return nullptr;
    }

    jlong tag() {
        return tag_;
    }

    jvmtiHeapReferenceKind kind() {
        return kind_;
    }

private:
    jlong tag_;
    jvmtiHeapReferenceKind kind_;
};

class infoWithIndex : public referenceInfo {
public:
    infoWithIndex(jlong tag, jvmtiHeapReferenceKind kind, jint index) : referenceInfo(tag, kind), index_(index) {}

    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override {
        return toJavaArray(env, index_);
    }

private:
    jint index_;
};

static jlongArray buildStackInfo(JNIEnv *env, jlong threadId, jint depth, jint slot) {
    std::vector<jlong> vector = {threadId, depth, slot};
    return toJavaArray(env, vector);
}

static jobjectArray buildMethodInfo(JNIEnv *env, jvmtiEnv *jvmti, jmethodID id) {
    if (id == nullptr) {
        return nullptr;
    }
    char *name, *signature, *genericSignature;
    jvmtiError err = jvmti->GetMethodName(id, &name, &signature, &genericSignature);
    handleError(jvmti, err, "Could not receive method info");
    if (err != JVMTI_ERROR_NONE)
        return nullptr;
    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, env->NewStringUTF(name));
    env->SetObjectArrayElement(result, 1, env->NewStringUTF(signature));
    env->SetObjectArrayElement(result, 2, env->NewStringUTF(genericSignature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(name));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));
    jvmti->Deallocate(reinterpret_cast<unsigned char *>(genericSignature));
    return result;
}

class stackInfo : public referenceInfo {
public:
    stackInfo(jlong tag, jvmtiHeapReferenceKind kind, jlong threadId, jint slot, jint depth, jmethodID methodId)
            : referenceInfo(tag, kind), threadId_(threadId), slot_(slot), depth_(depth), methodId_(methodId) {}

public:
    jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) override {
        return wrapWithArray(env,
                             buildStackInfo(env, threadId_, depth_, slot_),
                             buildMethodInfo(env, jvmti, methodId_)
        );
    }

private:
    jlong threadId_;
    jint slot_;
    jint depth_;
    jmethodID methodId_;
};

typedef struct PathNodeTag {
    std::vector<referenceInfo *> backRefs;

    ~PathNodeTag() {
        for (auto info : backRefs) {
            delete info;
        }
    }
} GcTag;

GcTag *createGcTag() {
    auto *tag = new GcTag();
    return tag;
}

GcTag *pointerToGcTag(jlong tagPtr) {
    if (tagPtr == 0) {
        return new GcTag();
    }
    return (GcTag *) (ptrdiff_t) (void *) tagPtr;
}

void cleanTag(jlong tag) {
    delete pointerToGcTag(tag);
}

referenceInfo *createReferenceInfo(jlong tag, jvmtiHeapReferenceKind kind, const jvmtiHeapReferenceInfo *info) {
    switch (kind) {
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD:
        case JVMTI_HEAP_REFERENCE_FIELD:
            return new infoWithIndex(tag, kind, info->field.index);
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT:
            return new infoWithIndex(tag, kind, info->array.index);
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
            return new infoWithIndex(tag, kind, info->constant_pool.index);
        case JVMTI_HEAP_REFERENCE_STACK_LOCAL: {
            jvmtiHeapReferenceInfoStackLocal const &stackLocal = info->stack_local;
            return new stackInfo(tag, kind, stackLocal.thread_id, stackLocal.slot, stackLocal.depth, stackLocal.method);
        }
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL: {
            jvmtiHeapReferenceInfoJniLocal const &jniLocal = info->jni_local;
            return new stackInfo(tag, kind, jniLocal.thread_id, -1, jniLocal.depth, jniLocal.method);
        }
            // no information provided
        case JVMTI_HEAP_REFERENCE_CLASS:
        case JVMTI_HEAP_REFERENCE_CLASS_LOADER:
        case JVMTI_HEAP_REFERENCE_SIGNERS:
        case JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN:
        case JVMTI_HEAP_REFERENCE_INTERFACE:
        case JVMTI_HEAP_REFERENCE_SUPERCLASS:
        case JVMTI_HEAP_REFERENCE_JNI_GLOBAL:
        case JVMTI_HEAP_REFERENCE_SYSTEM_CLASS:
        case JVMTI_HEAP_REFERENCE_MONITOR:
        case JVMTI_HEAP_REFERENCE_THREAD:
        case JVMTI_HEAP_REFERENCE_OTHER:
            break;
    }

    return new referenceInfo(tag, kind);
}

extern "C"
JNIEXPORT jint cbGcPaths(jvmtiHeapReferenceKind referenceKind,
                         const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                         jlong referrerClassTag, jlong size, jlong *tagPtr,
                         jlong *referrerTagPtr, jint length, void *userData) {

    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(createGcTag());
    }

    PathNodeTag *tag = pointerToGcTag(*tagPtr);
    if (referrerTagPtr != nullptr) {
        if (*referrerTagPtr == 0) {
            *referrerTagPtr = pointerToTag(createGcTag());
        }

        tag->backRefs.push_back(createReferenceInfo(*referrerTagPtr, referenceKind, referenceInfo));
    } else {
        // gc root found
        tag->backRefs.push_back(createReferenceInfo(-1, referenceKind, referenceInfo));
    }
    return JVMTI_VISIT_OBJECTS;
}

static void walk(jlong start, std::set<jlong> &visited, jint limit) {
    std::queue<jlong> queue;
    queue.push(start);
    jlong tag;
    while (!queue.empty() && visited.size() <= limit) {
        tag = queue.front();
        queue.pop();
        visited.insert(tag);
        for (referenceInfo *info: pointerToGcTag(tag)->backRefs) {
            jlong parentTag = info->tag();
            if (parentTag != -1 && visited.find(parentTag) == visited.end()) {
                queue.push(parentTag);
            }
        }
    }
}

static jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti,
                                     std::unordered_map<jlong, jint> tagToIndex,
                                     const std::vector<referenceInfo *> &infos) {
    std::vector<jint> prevIndices;
    std::vector<jint> refKinds;
    std::vector<jobject> refInfos;

    jint exceedLimitCount = 0;
    for (referenceInfo *info : infos) {
        jlong prevTag = info->tag();
        if (prevTag != -1 && tagToIndex.find(prevTag) == tagToIndex.end()) {
            ++exceedLimitCount;
            continue;
        }
        prevIndices.push_back(prevTag == -1 ? -1 : tagToIndex[prevTag]);
        refKinds.push_back(static_cast<jint>(info->kind()));
        refInfos.push_back(info->getReferenceInfo(env, jvmti));
    }

    jobjectArray result = env->NewObjectArray(4, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));
    env->SetObjectArrayElement(result, 3, toJavaArray(env, exceedLimitCount));

    return result;
}

static jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti, jlong tag,
                                     std::unordered_map<jlong, jint> &tagToIndex) {
    return createLinksInfos(env, jvmti, tagToIndex, pointerToGcTag(tag)->backRefs);
}

static std::vector<std::pair<jobject, jlong>> getObjectToTag(jvmtiEnv *jvmti, std::vector<jlong> &tags) {
    std::vector<std::pair<jobject, jlong>> objectToTag;
    jvmtiError err = cleanHeapAndGetObjectsByTags(jvmti, tags, objectToTag, cleanTag);
    handleError(jvmti, err, "Could not receive objects by their tags");

    return objectToTag;
}

static std::unordered_map<jlong, jint> getTagToIndex(const std::vector<std::pair<jobject, jlong>> &objectToTag) {
    std::unordered_map<jlong, jint> tagToIndex;
    for (jsize i = 0; i < objectToTag.size(); ++i) {
        tagToIndex[objectToTag[i].second] = i;
    }

    return tagToIndex;
}

static jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti,
                                       const std::vector<std::pair<jobject, jlong>> &objectToTag) {
    std::unordered_map<jlong, jint> tagToIndex = getTagToIndex(objectToTag);
    jclass langObject = env->FindClass("java/lang/Object");
    jint objectsCount = static_cast<jint>(objectToTag.size());
    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);
    for (jsize i = 0; i < objectsCount; ++i) {
        env->SetObjectArrayElement(resultObjects, i, objectToTag[i].first);
        auto infos = createLinksInfos(env, jvmti, objectToTag[i].second, tagToIndex);
        env->SetObjectArrayElement(links, i, infos);
    }

    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    env->SetObjectArrayElement(result, 0, resultObjects);
    env->SetObjectArrayElement(result, 1, links);

    return result;
}

static jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &&tags) {
    return createResultObject(env, jvmti, getObjectToTag(jvmti, tags));
}

static GcTag * createTags(jvmtiEnv *jvmti, jobject target) {
    jvmtiError err;
    jvmtiHeapCallbacks cb;

    memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbGcPaths);

    GcTag *tag = createGcTag();
    err = jvmti->SetTag(target, pointerToTag(tag));
    handleError(jvmti, err, "Could not set tag for target object");
    info("start following through references");
    err = jvmti->FollowReferences(JVMTI_HEAP_OBJECT_EITHER, nullptr, nullptr, &cb, nullptr);
    handleError(jvmti, err, "FollowReference call failed");
    info("heap tagged");

    return tag;
}

jobjectArray findGcRoots(JNIEnv *jni, jvmtiEnv *jvmti, jobject object, jint limit) {
    info("Looking for paths to gc roots started");
    GcTag * tag = createTags(jvmti, object);

    info("heap tagged");
    std::set<jlong> uniqueTags;
    info("start walking through collected tags");
    walk(pointerToTag(tag), uniqueTags, limit);

    info("create resulting java objects");
    std::unordered_map<jlong, jint> tag_to_index;
    jobjectArray result = createResultObject(jni, jvmti,
            std::vector<jlong>(uniqueTags.begin(), uniqueTags.end()));

    info("remove all tags from objects in heap");
    jvmtiError err = removeAllTagsFromHeap(jvmti, cleanTag);
    handleError(jvmti, err, "Count not remove all tags");

    return result;
}

static referenceInfo *getReferrerInfo(jlong referee, jlong referrer) {
    GcTag *tag = pointerToGcTag(referee);
    for (referenceInfo *info : tag->backRefs) {
        if (info->tag() == referrer) {
            return info;
        }
    }

    return nullptr;
}

static std::vector<std::pair<jobject, jlong>> getSortedObjectToTag(jvmtiEnv *jvmti,
                                                                   const std::vector<std::pair<jlong, jint>> &nodes) {
    std::unordered_map<jlong, jint> tagToPathNumber;
    std::vector<jlong> tags(nodes.size());
    for (auto node : nodes) {
        tags.push_back(node.first);
        tagToPathNumber[node.first] = node.second;
    }

    std::vector<std::pair<jobject, jlong>> objectToTag = getObjectToTag(jvmti, tags);
    std::sort(
            objectToTag.begin(),
            objectToTag.end(),
            [&tagToPathNumber](const auto &left, const auto &right) {
                return tagToPathNumber[left.second] < tagToPathNumber[right.second] ;
            }
    );

    return objectToTag;
}

static jobjectArray createResultObjectForGcRootsPaths(JNIEnv *env, jvmtiEnv *jvmti,
                                                      const std::vector<std::pair<jlong, jint>> &nodes,
                                                      std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {
    std::vector<std::pair<jobject, jlong>> objectToTag = getSortedObjectToTag(jvmti, nodes);
    std::unordered_map<jlong, jint> tagToIndex = getTagToIndex(objectToTag);
    jint objectsCount = static_cast<jint>(objectToTag.size());
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);

    for (jsize i = 0; i < objectsCount; ++i) {
        env->SetObjectArrayElement(resultObjects, i, objectToTag[i].first);
        auto infos = createLinksInfos(env, jvmti, tagToIndex, tagToInfos[objectToTag[i].second]);
        env->SetObjectArrayElement(links, i, infos);
    }

    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    env->SetObjectArrayElement(result, 0, resultObjects);
    env->SetObjectArrayElement(result, 1, links);

    return result;
}

static bool insertInfos(jlong referee, jlong referrer,
                        std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {
    referenceInfo *info = getReferrerInfo(referee, referrer);
    auto it = tagToInfos.find(referee);
    if (it == tagToInfos.end()) {
        tagToInfos[referee] = std::vector<referenceInfo*>{info};
        return false;
    }

    it->second.push_back(info);
    return true;
}

static jobjectArray createResultObjectForGcRootsPaths(JNIEnv *env, jvmtiEnv *jvmti,
                                                      const std::vector<jlong> &roots,
                                                      std::unordered_map<jlong, jlong> &prevNode) {
    std::vector<std::pair<jlong, jint>> nodes;
    std::unordered_map<jlong, std::vector<referenceInfo *>> tagToInfos;
    jint cnt = 0;
    for (jlong tag : roots) {
        nodes.emplace_back(tag, cnt++);
        insertInfos(tag, -1, tagToInfos);
        while (prevNode[tag] != tag) {
            jlong prevTag = prevNode[tag];

            if (insertInfos(prevTag, tag, tagToInfos)) {
                break;
            }

            nodes.emplace_back(prevTag, cnt);
            tag = prevTag;
        }
    }

    return createResultObjectForGcRootsPaths(env, jvmti, nodes, tagToInfos);
}

static bool isValidGcRoot(jlong tag, jint kind) {
    return tag == -1 &&
           kind != JVMTI_HEAP_REFERENCE_JNI_GLOBAL &&
           kind != JVMTI_HEAP_REFERENCE_JNI_LOCAL;
}

static jobjectArray collectPathsToClosestGcRoots(JNIEnv *env, jvmtiEnv *jvmti,
                                                 jlong start, jint number) {
    std::queue<jlong> queue;
    std::unordered_map<jlong, jlong> prevTag;
    std::unordered_set<jlong> foundRootsSet;
    std::vector<jlong> foundRoots;

    jlong tag = start;
    queue.push(tag);
    prevTag[tag] = tag;
    while (!queue.empty() && number > foundRoots.size()) {
        tag = queue.front();
        queue.pop();
        for (referenceInfo *info : pointerToGcTag(tag)->backRefs) {
            jlong parentTag = info->tag();
            if (isValidGcRoot(parentTag, info->kind()) &&
                foundRootsSet.insert(tag).second) {
                foundRoots.push_back(tag);
            } else if (parentTag != -1 && prevTag.find(parentTag) == prevTag.end()) {
                prevTag[parentTag] = tag;
                queue.push(parentTag);
            }

            if (foundRoots.size() >= number) {
                break;
            }
        }
    }


    return foundRoots.empty() ?
            createResultObject(env, jvmti, std::vector<jlong>{start}) :
            createResultObjectForGcRootsPaths(env, jvmti, foundRoots, prevTag);
}

jobjectArray findPathsToClosestGcRoots(JNIEnv *env, jvmtiEnv *jvmti, jobject object, jint number) {
    info("Looking for shortest path to gc root started");
    GcTag * tag = createTags(jvmti, object);

    info("create resulting java objects");
    jobjectArray result = collectPathsToClosestGcRoots(env, jvmti, pointerToTag(tag), number);

    info("remove all tags from objects in heap");
    jvmtiError err = removeAllTagsFromHeap(jvmti, cleanTag);
    handleError(jvmti, err, "Count not remove all tags");

    return result;
}
