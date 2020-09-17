// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "sizes_tags.h"
#include "sizes_callbacks.h"
#include "../timed_action.h"
#include "../utils.h"

std::unordered_set<jlong> tagsWithNewInfo;

static bool handleReferrersWithNoInfo(const jlong *referrerTagPtr, jlong *tagPtr, bool setTagsWithNewInfo=false) {
    if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr)) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(&Tag::EmptyTag);
        } else if (!isEmptyTag(*tagPtr)) {
            Tag *referree = tagToPointer(*tagPtr);
            if (setTagsWithNewInfo && referree->alreadyReferred) {
                referree->unref();
                *tagPtr = pointerToTag(&Tag::TagWithNewInfo);
            } else {
                referree->visitFromUntaggedReferrer();
            }
        }

        return true;
    } else if (isTagWithNewInfo(*referrerTagPtr) || *referrerTagPtr == 0 ||
               tagToPointer(*referrerTagPtr)->getId() > 0) {
        return true;
    }

    return false;
}

static bool tagsAreValidForMerge(jlong referree, jlong referrer) {
    return referree != referrer && shouldMerge(referree, referrer) &&
           tagToPointer(referree)->getId() == 0;
}

jint JNICALL getTagsWithNewInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                jlong *referrerTagPtr, jint length, void *userData) {
    if (shouldStopIteration(userData)) {
        return JVMTI_VISIT_ABORT;
    }

    if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL ||
        isTagWithNewInfo(*tagPtr) || handleReferrersWithNoInfo(referrerTagPtr, tagPtr, true)) {
        return JVMTI_VISIT_OBJECTS;
    }

    Tag *referrer = tagToPointer(*referrerTagPtr);
    Tag *referree = tagToPointer(*tagPtr);
    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(referrer->share());
    } else if (tagsAreValidForMerge(*tagPtr, *referrerTagPtr)) {
        if (referree->alreadyReferred) {
            referree->unref();
            *tagPtr = pointerToTag(&Tag::TagWithNewInfo);
        } else {
            *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
        }
    }

    referrer->alreadyReferred = true;
    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *userData) {
    if (shouldStopIteration(userData)) {
        return JVMTI_VISIT_ABORT;
    }

    if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL ||
        handleReferrersWithNoInfo(referrerTagPtr, tagPtr)) {
        return JVMTI_VISIT_OBJECTS;
    } else if (*tagPtr == 0) {
        *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());
    } else if (isTagWithNewInfo(*tagPtr)) {
        *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());
        tagsWithNewInfo.insert(*tagPtr);
    } else if (tagsAreValidForMerge(*tagPtr, *referrerTagPtr)) {
        tagsWithNewInfo.erase(*tagPtr);
        *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
        tagsWithNewInfo.insert(*tagPtr);
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL spreadInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                        jlong *referrerTagPtr, jint length, void *userData) {
    if (shouldStopIteration(userData)) {
        return JVMTI_VISIT_ABORT;
    }

    if (refKind != JVMTI_HEAP_REFERENCE_JNI_LOCAL && refKind != JVMTI_HEAP_REFERENCE_JNI_GLOBAL &&
        *tagPtr != 0 && *referrerTagPtr != 0) {
        auto it = tagsWithNewInfo.find(*tagPtr);
        if (it != tagsWithNewInfo.end()) {
            tagsWithNewInfo.erase(it);
        }

        if (*referrerTagPtr != *tagPtr && shouldMerge(*tagPtr, *referrerTagPtr)) {
            *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
        }
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL clearTag(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    tagToPointer(*tagPtr)->unref();
    *tagPtr = 0;

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL retagStartObjects(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    Tag *pClassTag = tagToPointer(classTag);
    if (classTag != 0 && pClassTag->getId() > 0 && isTagWithNewInfo(*tagPtr)) {
        *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
        tagsWithNewInfo.insert(*tagPtr);
    }

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL tagObjectOfTaggedClass(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    Tag *pClassTag = tagToPointer(classTag);
    if (classTag != 0 && pClassTag->getId() > 0) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
        } else {
            Tag *oldTag = tagToPointer(*tagPtr);
            *tagPtr = pointerToTag(ClassTag::create(pClassTag->getId() - 1, createState(true, true, false, false),
                                                    oldTag->getId()));
            delete oldTag;
        }
    }

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    for (query_size_t i = 0; i < tag->array.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag->array[i];
        if (isRetained(info.state)) {
            reinterpret_cast<jlong *>(userData)[info.index] += size;
        }
    }

    return JVMTI_ITERATION_CONTINUE;
}

jvmtiError walkHeapFromObjects(jvmtiEnv *jvmti,
                               const std::vector<std::pair<jobject, jlong>> &objectsAndTags,
                               const std::chrono::steady_clock::time_point &finishTime) {
    jvmtiError err = JVMTI_ERROR_NONE;
    if (!objectsAndTags.empty()) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&spreadInfo);
        int heapWalksCnt = 0;
        for (auto &objectAndTag : objectsAndTags) {
            if (finishTime < std::chrono::steady_clock::now()) {
                return MEMORY_AGENT_TIMEOUT_ERROR;
            }

            jlong tag;
            err = jvmti->GetTag(objectAndTag.first, &tag);
            if (err != JVMTI_ERROR_NONE) return err;

            if (tagsWithNewInfo.find(tag) != tagsWithNewInfo.end()) {
                tagsWithNewInfo.erase(tag);
                err = jvmti->FollowReferences(0, nullptr, objectAndTag.first, &cb, nullptr);
                if (err != JVMTI_ERROR_NONE) return err;
                debug(std::to_string(tagsWithNewInfo.size()).c_str());
                heapWalksCnt++;
            }
        }
        debug(std::string("Total heap walks: " + std::to_string(heapWalksCnt)).c_str());
    }

    return err;
}