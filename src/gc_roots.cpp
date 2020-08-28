// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_map>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include "gc_roots.h"
#include "utils.h"
#include "log.h"

static jlong tagBalance = 0;

class referenceInfo {
public:
    referenceInfo(jlong tag, jvmtiHeapReferenceKind kind) : tag_(tag), kind_(kind) {}

    virtual ~referenceInfo() = default;

    virtual jobject getReferenceInfo(JNIEnv *env, jvmtiEnv *jvmti) {
        return nullptr;
    }

    jlong tag() const {
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

class State {
public:
    State() = default;

    State(bool isAlreadyVisited, bool isWeakSoftReachable) : state(0) {
        setAlreadyVisited(isAlreadyVisited);
        setWeakSoftReachable(isWeakSoftReachable);
    }

    void setAlreadyVisited(bool value) {
        setAttribute(value, 0);
    }

    void setWeakSoftReachable(bool value) {
        setAttribute(value, 1u);
    }

    bool isAlreadyVisited() const {
        return checkAttribute(0u);
    }

    bool isWeakSoftReachable() const {
        return checkAttribute(1u);
    }

    void updateWeakSoftReachableValue(const State &referrerState) {
        if (isWeakSoftReachable()) {
            if (!referrerState.isWeakSoftReachable()) {
                setWeakSoftReachable(false);
            }
        } else {
            if (referrerState.isWeakSoftReachable() && !isAlreadyVisited()) {
                setWeakSoftReachable(true);
            }
        }
    }

private:
    void setAttribute(bool value, uint8_t offset) {
        state = value ? state | (1u << offset) : state & ~(1u << offset);
    }

    bool checkAttribute(uint8_t offset) const {
        return (state & (1u << offset)) != 0u;
    }

private:
    uint8_t state;
};

struct GcTag {
    GcTag() : state() {
    }

    explicit GcTag(bool isWeakSoftReachable) :
        state(false, isWeakSoftReachable) {

    }

    ~GcTag() {
        --tagBalance;
        for (auto info : backRefs) {
            delete info;
        }
    }

    static GcTag *create(jlong classTag) {
        ++tagBalance;
        return new GcTag(classTag != 0 && pointerToGcTag(classTag)->isWeakSoftReachable());
    }

    static GcTag *create() {
        ++tagBalance;
        return new GcTag();
    }

    static GcTag *pointerToGcTag(jlong tagPtr) {
        if (tagPtr == 0) {
            return new GcTag();
        }
        return (GcTag *) (ptrdiff_t) (void *) tagPtr;
    }

    static void cleanTag(jlong tag) {
        delete pointerToGcTag(tag);
    }

    bool isWeakSoftReachable() const {
        return state.isWeakSoftReachable();
    }

    void setVisited() {
        state.setAlreadyVisited(true);
    }

    virtual void updateState(const GcTag  *const referrer) {
        state.updateWeakSoftReachableValue(referrer->state);
    }

public:
    std::vector<referenceInfo *> backRefs;

private:
    State state;
};

struct WeakSoftReferenceClassTag : public GcTag {
    WeakSoftReferenceClassTag() : GcTag(true) {

    }

    void updateState(const GcTag  *const referrer) override {

    }

    static GcTag *create() {
        ++tagBalance;
        return new WeakSoftReferenceClassTag();
    }
};

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

jint JNICALL cbGcPaths(jvmtiHeapReferenceKind referenceKind,
                         const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                         jlong referrerClassTag, jlong size, jlong *tagPtr,
                         const jlong *referrerTagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(GcTag::create(referrerClassTag));
    }

    GcTag *tag = GcTag::pointerToGcTag(*tagPtr);
    if (referrerTagPtr != nullptr) {
        if (referrerClassTag != 0 && GcTag::pointerToGcTag(referrerClassTag)->isWeakSoftReachable()) {
            tag->updateState(GcTag::pointerToGcTag(referrerClassTag));
        } else {
            tag->updateState(GcTag::pointerToGcTag(*referrerTagPtr));
        }

        tag->backRefs.push_back(createReferenceInfo(*referrerTagPtr, referenceKind, referenceInfo));
    } else {
        // gc root found
        tag->backRefs.push_back(createReferenceInfo(-1, referenceKind, referenceInfo));
    }

    tag->setVisited();
    return JVMTI_VISIT_OBJECTS;
}

static jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti,
                                     const std::unordered_map<jlong, jint> &tagToIndex,
                                     const std::vector<referenceInfo *> &infos) {
    std::vector<jint> prevIndices;
    std::vector<jint> refKinds;
    std::vector<jobject> refInfos;

    for (referenceInfo *info : infos) {
        jlong prevTag = info->tag();
        auto it = tagToIndex.find(prevTag);
        if (prevTag != -1 && it == tagToIndex.end()) {
            continue;
        }
        prevIndices.push_back(prevTag == -1 ? -1 : it->second);
        refKinds.push_back(static_cast<jint>(info->kind()));
        refInfos.push_back(info->getReferenceInfo(env, jvmti));
    }

    jobjectArray result = env->NewObjectArray(4, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
    env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));

    return result;
}

static std::vector<std::pair<jobject, jlong>> getObjectToTag(jvmtiEnv *jvmti, std::vector<jlong> &tags) {
    std::vector<std::pair<jobject, jlong>> objectToTag;
    jvmtiError err = cleanHeapAndGetObjectsByTags(jvmti, tags, objectToTag, GcTag::cleanTag);
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
                                       const std::vector<std::pair<jobject, jlong>> &objectToTag,
                                       const std::unordered_map<jlong, std::vector<referenceInfo *>> *tagToInfos=nullptr) {
    std::unordered_map<jlong, jint> tagToIndex = getTagToIndex(objectToTag);
    jclass langObject = env->FindClass("java/lang/Object");
    jint objectsCount = static_cast<jint>(objectToTag.size());
    jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);
    jobjectArray links = env->NewObjectArray(objectsCount, langObject, nullptr);
    std::vector<jboolean> weakSoftReachable(objectsCount);

    for (jsize i = 0; i < objectsCount; ++i) {
        env->SetObjectArrayElement(resultObjects, i, objectToTag[i].first);
        jlong tag = objectToTag[i].second;
        auto infos = tagToInfos == nullptr ?
                createLinksInfos(env, jvmti, tagToIndex, GcTag::pointerToGcTag(tag)->backRefs) :
                createLinksInfos(env, jvmti, tagToIndex, (*tagToInfos).find(tag)->second);
        env->SetObjectArrayElement(links, i, infos);
        weakSoftReachable[i] = GcTag::pointerToGcTag(tag)->isWeakSoftReachable();
    }

    jobjectArray result = env->NewObjectArray(3, langObject, nullptr);
    env->SetObjectArrayElement(result, 0, resultObjects);
    env->SetObjectArrayElement(result, 1, links);
    env->SetObjectArrayElement(result, 2, toJavaArray(env, weakSoftReachable));

    return result;
}

static jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &&tags) {
    return createResultObject(env, jvmti, getObjectToTag(jvmti, tags));
}

static void setTagsForReferences(JNIEnv *env, jvmtiEnv *jvmti) {
    std::string classes[] = {
            "java/lang/ref/SoftReference",
            "java/lang/ref/WeakReference",
            "java/lang/ref/PhantomReference"
    };

    for (const std::string &klass : classes) {
        jvmtiError err = jvmti->SetTag(
                env->FindClass(klass.data()),
                pointerToTag(WeakSoftReferenceClassTag::create())
        );
        handleError(jvmti, err, ("Could not set tag for " + klass + " class").data());
    }
}

static GcTag *createTags(JNIEnv *env, jvmtiEnv *jvmti, jobject target) {
    jvmtiError err;
    jvmtiHeapCallbacks cb;

    memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&cbGcPaths);

    setTagsForReferences(env, jvmti);

    GcTag *tag = GcTag::create();
    err = jvmti->SetTag(target, pointerToTag(tag));
    handleError(jvmti, err, "Could not set tag for target object");

    info("start following through references");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    handleError(jvmti, err, "FollowReference call failed");
    info("heap tagged");

    return tag;
}

static referenceInfo *getReferrerInfo(jlong referee, jlong referrer) {
    GcTag *tag = GcTag::pointerToGcTag(referee);
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

static void insertRootInfos(jlong referee, std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {;
    std::vector<referenceInfo *> *pInfos;
    auto it = tagToInfos.find(referee);
    if (it == tagToInfos.end()) {
        tagToInfos[referee] = std::vector<referenceInfo *>();
        pInfos = &tagToInfos[referee];
    } else {
        pInfos = &it->second;
    }

    GcTag *tag = GcTag::pointerToGcTag(referee);
    for (referenceInfo *info : tag->backRefs) {
        if (info->tag() == -1) {
            pInfos->push_back(info);
        }
    }
}

static bool insertInfos(jlong referee, jlong referrer,
                        std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {
    referenceInfo *info = getReferrerInfo(referee, referrer);
    auto it = tagToInfos.find(referee);
    if (it == tagToInfos.end()) {
        tagToInfos[referee] = std::vector<referenceInfo *>{info};
        return false;
    }

    it->second.push_back(info);
    return true;
}

static bool insertTruncatedReferenceInfo(jlong start, jlong referrer, jint lengthToStart,
                                         std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {
    referenceInfo *info = new infoWithIndex(referrer, static_cast<jvmtiHeapReferenceKind>(MEMORY_AGENT_TRUNCATE_REFERENCE), lengthToStart);
    auto it = tagToInfos.find(start);
    if (it == tagToInfos.end()) {
        tagToInfos[start] = std::vector<referenceInfo *>{info};
        return false;
    }
    it->second.push_back(info);
    return true;
}

static void truncatePath(jlong start, jlong tag,  const std::unordered_map<jlong, jlong> &prevNode,
                         std::unordered_map<jlong, std::vector<referenceInfo *>> &tagToInfos) {
    jint lengthToStart = 0;
    jlong oldTag = tag;
    jlong prevTag = prevNode.find(tag)->second;
    while (prevTag != tag) {
        tag = prevTag;
        prevTag = prevNode.find(tag)->second;
        lengthToStart++;
    }
    insertTruncatedReferenceInfo(start, oldTag, lengthToStart, tagToInfos);
}

static jobjectArray createResultObjectForGcRootsPaths(JNIEnv *env, jvmtiEnv *jvmti, const std::vector<jlong> &roots,
                                                      const std::unordered_map<jlong, jlong> &prevNode, jlong start, jint objectsNumber) {
    std::vector<std::pair<jlong, jint>> nodesToPathNum;
    std::unordered_map<jlong, std::vector<referenceInfo *>> tagToInfos;
    jint cnt = 0;
    for (auto it = roots.begin(); it != roots.end() && nodesToPathNum.size() < objectsNumber; ++it) {
        jlong tag = *it;
        nodesToPathNum.emplace_back(tag, cnt++);
        insertRootInfos(tag, tagToInfos);
        while (true) {
            if (nodesToPathNum.size() >= objectsNumber - roots.size()) {
                if (cnt == 1) {
                    nodesToPathNum.emplace_back(start, 1);
                }

                truncatePath(start, tag, prevNode, tagToInfos);
                break;
            }

            jlong prevTag = prevNode.find(tag)->second;
            if (prevTag == tag || insertInfos(prevTag, tag, tagToInfos)) {
                break;
            }

            nodesToPathNum.emplace_back(prevTag, cnt);
            tag = prevTag;
        }
    }

    std::vector<std::pair<jobject, jlong>> objectToTag = getSortedObjectToTag(jvmti, nodesToPathNum);
    return createResultObject(env, jvmti, objectToTag, &tagToInfos);
}

static bool isValidGcRoot(jlong tag, jint kind) {
    return tag == -1 &&
           kind != JVMTI_HEAP_REFERENCE_JNI_GLOBAL &&
           kind != JVMTI_HEAP_REFERENCE_JNI_LOCAL;
}

static jobjectArray collectPathsToClosestGcRoots(JNIEnv *env, jvmtiEnv *jvmti, jlong start,
                                                 jint number, jint objectsNumber) {
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
        for (referenceInfo *info : GcTag::pointerToGcTag(tag)->backRefs) {
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
            createResultObjectForGcRootsPaths(env, jvmti, foundRoots, prevTag, start, objectsNumber);
}

jobjectArray findPathsToClosestGcRoots(JNIEnv *env, jvmtiEnv *jvmti, jobject object, jint pathsNumber, jint objectsNumber) {
    info("Looking for shortest path to gc root started");
    GcTag *tag = createTags(env, jvmti, object);

    info("create resulting java objects");
    jobjectArray result = collectPathsToClosestGcRoots(env, jvmti, pointerToTag(tag), pathsNumber, objectsNumber);

    info("remove all tags from objects in heap");
    jvmtiError err = removeAllTagsFromHeap(jvmti, GcTag::cleanTag);
    handleError(jvmti, err, "Count not remove all tags");

    if (tagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    return result;
}
