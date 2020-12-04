// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <queue>
#include "paths_to_closest_gc_roots.h"
#include "roots_tags.h"

namespace {
    jint JNICALL collectPaths(jvmtiHeapReferenceKind referenceKind,
                              const jvmtiHeapReferenceInfo *referenceInfo, jlong classTag,
                              jlong referrerClassTag, jlong size, jlong *tagPtr,
                              jlong *referrerTagPtr, jint length, void *userData) {
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

    jobjectArray createLinksInfos(JNIEnv *env, jvmtiEnv *jvmti,
                                  const std::unordered_map<jlong, jint> &tagToIndex,
                                  const std::vector<ReferenceInfo *> &infos) {
        std::vector<jint> prevIndices;
        std::vector<jint> refKinds;
        std::vector<jobject> refInfos;

        size_t size = infos.size();
        prevIndices.reserve(size);
        refInfos.reserve(size);
        refKinds.reserve(size);
        for (ReferenceInfo *info : infos) {
            jlong prevTag = info->getTag();
            auto it = tagToIndex.find(prevTag);
            if (prevTag != -1 && it == tagToIndex.end()) {
                continue;
            }
            prevIndices.push_back(prevTag == -1 ? -1 : it->second);
            refKinds.push_back(static_cast<jint>(info->getKind()));
            refInfos.push_back(info->getReferenceInfo(env, jvmti));
        }

        jobjectArray result = env->NewObjectArray(4, env->FindClass("java/lang/Object"), nullptr);
        env->SetObjectArrayElement(result, 0, toJavaArray(env, prevIndices));
        env->SetObjectArrayElement(result, 1, toJavaArray(env, refKinds));
        env->SetObjectArrayElement(result, 2, toJavaArray(env, refInfos));

        return result;
    }

    std::vector<std::pair<jobject, jlong>> getObjectToTag(jvmtiEnv *jvmti, std::vector<jlong> &tags) {
        std::vector<std::pair<jobject, jlong>> objectToTag;
        jvmtiError err = getObjectsByTags(jvmti, tags, objectToTag);
        handleError(jvmti, err, "Could not receive objects by their tags");

        return objectToTag;
    }

    std::unordered_map<jlong, jint> getTagToIndex(const std::vector<std::pair<jobject, jlong>> &objectToTag) {
        std::unordered_map<jlong, jint> tagToIndex;
        for (jsize i = 0; i < objectToTag.size(); ++i) {
            tagToIndex[objectToTag[i].second] = i;
        }

        return tagToIndex;
    }

    jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti,
                                    const std::vector<std::pair<jobject, jlong>> &objectToTag,
                                    const std::unordered_map<jlong, std::vector<ReferenceInfo *>> *tagToInfos = nullptr) {
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

    jobjectArray createResultObject(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jlong> &&tags) {
        return createResultObject(env, jvmti, getObjectToTag(jvmti, tags));
    }

    ReferenceInfo *getReferrerInfo(jlong referee, jlong referrer) {
        GcTag *tag = GcTag::pointerToGcTag(referee);
        for (ReferenceInfo *info : tag->backRefs) {
            if (info->getTag() == referrer) {
                return info;
            }
        }

        return nullptr;
    }

    std::vector<std::pair<jobject, jlong>> getSortedObjectToTag(jvmtiEnv *jvmti,
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
                    return tagToPathNumber[left.second] < tagToPathNumber[right.second];
                }
        );

        return objectToTag;
    }

    void insertRootInfos(jlong referee, std::unordered_map<jlong, std::vector<ReferenceInfo *>> &tagToInfos) {
        std::vector<ReferenceInfo *> *pInfos;
        auto it = tagToInfos.find(referee);
        if (it == tagToInfos.end()) {
            tagToInfos[referee] = std::vector<ReferenceInfo *>();
            pInfos = &tagToInfos[referee];
        } else {
            pInfos = &it->second;
        }

        GcTag *tag = GcTag::pointerToGcTag(referee);
        for (ReferenceInfo *info : tag->backRefs) {
            if (info->getTag() == -1) {
                pInfos->push_back(info);
            }
        }
    }

    bool insertInfos(jlong referee, jlong referrer,
                     std::unordered_map<jlong, std::vector<ReferenceInfo *>> &tagToInfos) {
        ReferenceInfo *info = getReferrerInfo(referee, referrer);
        auto it = tagToInfos.find(referee);
        if (it == tagToInfos.end()) {
            tagToInfos[referee] = std::vector<ReferenceInfo *>{info};
            return false;
        }

        it->second.push_back(info);
        return true;
    }

    bool insertTruncatedReferenceInfo(jlong start, jlong referrer, jint lengthToStart,
                                      std::unordered_map<jlong, std::vector<ReferenceInfo *>> &tagToInfos) {
        ReferenceInfo *info = new InfoWithIndex(referrer, MEMORY_AGENT_TRUNCATE_REFERENCE, lengthToStart);
        auto it = tagToInfos.find(start);
        if (it == tagToInfos.end()) {
            tagToInfos[start] = std::vector<ReferenceInfo *>{info};
            return false;
        }
        it->second.push_back(info);
        return true;
    }

    void truncatePath(jlong start, jlong tag, const std::unordered_map<jlong, jlong> &prevNode,
                      std::unordered_map<jlong, std::vector<ReferenceInfo *>> &tagToInfos) {
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

    jobjectArray getEmptyArray(JNIEnv *env, jint size=3) {
        return env->NewObjectArray(size, env->FindClass("java/lang/Object"), nullptr);
    }

    jobjectArray createResultObjectForGcRootsPaths(JNIEnv *env, jvmtiEnv *jvmti, const std::vector<jlong> &roots,
                                                   const std::unordered_map<jlong, jlong> &prevNode, jlong start,
                                                   jint objectsNumber) {
        std::vector<std::pair<jlong, jint>> nodesToPathNum;
        std::unordered_map<jlong, std::vector<ReferenceInfo *>> tagToInfos;
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

    bool isValidGcRoot(jlong tag, jint kind) {
        return tag == -1 &&
               kind != JVMTI_HEAP_REFERENCE_JNI_GLOBAL &&
               kind != JVMTI_HEAP_REFERENCE_JNI_LOCAL;
    }
}

PathsToClosestGcRootsAction::PathsToClosestGcRootsAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) : MemoryAgentTimedAction(env, jvmti, object) {

}

jobjectArray PathsToClosestGcRootsAction::collectPathsToClosestGcRoots(jlong start, jint number, jint objectsNumber) {
    std::queue<jlong> queue;
    std::unordered_map<jlong, jlong> prevTag;
    std::unordered_set<jlong> foundRootsSet;
    std::vector<jlong> foundRoots;

    jlong tag = start;
    queue.push(tag);
    prevTag[tag] = tag;
    while (!queue.empty() && number > foundRoots.size()) {
        if (shouldStopExecution()) return getEmptyArray(env);

        tag = queue.front();
        queue.pop();
        for (ReferenceInfo *info : GcTag::pointerToGcTag(tag)->backRefs) {
            jlong parentTag = info->getTag();
            if (isValidGcRoot(parentTag, info->getKind()) &&
                foundRootsSet.insert(tag).second) {
                foundRoots.push_back(tag);
            } else if (parentTag != -1 && prevTag.insert(std::make_pair(parentTag, tag)).second) {
                queue.push(parentTag);
            }

            if (foundRoots.size() >= number) {
                break;
            }
        }
    }

    if (shouldStopExecution()) return getEmptyArray(env);

    return foundRoots.empty() ?
           createResultObject(env, jvmti, std::vector<jlong>{start}) :
           createResultObjectForGcRootsPaths(env, jvmti, foundRoots, prevTag, start, objectsNumber);
}

void setTagsForReferences(JNIEnv *env, jvmtiEnv *jvmti, jlong tag) {
    const char *refClassesNames[3] = {
            "java/lang/ref/SoftReference",
            "java/lang/ref/WeakReference",
            "java/lang/ref/PhantomReference"
    };
    jclass refClasses[3];
    for (int i = 0; i < 3; i++) {
        refClasses[i] = env->FindClass(refClassesNames[i]);
    }

    jclass *classes;
    jint cnt;
    jvmtiError err = jvmti->GetLoadedClasses(&cnt, &classes);
    handleError(jvmti, err, "Couldn't get loaded classes");
    jclass langClass = env->FindClass("java/lang/Class");
    jmethodID isAssignableFrom = env->GetMethodID(langClass, "isAssignableFrom", "(Ljava/lang/Class;)Z");

    for (int i = 0; i < cnt; i++) {
        for (auto & refClass : refClasses) {
            if (env->CallBooleanMethod(refClass, isAssignableFrom, classes[i])) {
                err = jvmti->SetTag(classes[i], tag);
                handleError(jvmti, err, "Couldn't set tag for reference class");
                break;
            }
        }
    }
}

GcTag *PathsToClosestGcRootsAction::createTags(jobject target) {
    setTagsForReferences(env, jvmti, pointerToTag(&GcTag::WeakSoftReferenceTag));

    GcTag *tag = GcTag::create();
    jvmtiError err = jvmti->SetTag(target, pointerToTag(tag));
    handleError(jvmti, err, "Could not set getTag for target object");

    err = FollowReferences(0, nullptr, nullptr, collectPaths, nullptr, "collecting objects paths");
    handleError(jvmti, err, "FollowReference call failed");

    return tag;
}

jobjectArray PathsToClosestGcRootsAction::executeOperation(jobject object, jint pathsNumber, jint objectsNumber) {
    debug("Looking for shortest path to gc root started");
    GcTag *tag = createTags(object);
    if (shouldStopExecution()) return getEmptyArray(env);

    debug("create resulting java objects");
    jobjectArray result = collectPathsToClosestGcRoots(pointerToTag(tag), pathsNumber, objectsNumber);

    return result;
}

jvmtiError PathsToClosestGcRootsAction::cleanHeap() {
    debug("remove all tags from objects in heap");
    std::set<jlong> ignoredTag {pointerToTag(&GcTag::WeakSoftReferenceTag)};
    jvmtiError err = removeTagsFromHeap(jvmti, ignoredTag, GcTag::cleanTag);

    if (rootsTagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    return err;
}
