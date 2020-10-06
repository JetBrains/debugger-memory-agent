// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "retained_size_action.h"

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
               tagToClassTagPointer(*referrerTagPtr)) {
        return true;
    }

    return false;
}

static bool tagsAreValidForMerge(jlong referree, jlong referrer) {
    return !tagToClassTagPointer(referree) && referree != referrer && shouldMerge(referree, referrer);
}

jint JNICALL getTagsWithNewInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                jlong *referrerTagPtr, jint length, void *userData) {
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
    ClassTag *pClassTag = tagToClassTagPointer(classTag);
    if (pClassTag && isTagWithNewInfo(*tagPtr)) {
        *tagPtr = pointerToTag(pClassTag->createStartTag());
        tagsWithNewInfo.insert(*tagPtr);
    }

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL tagObjectOfTaggedClass(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    ClassTag *pClassTag = tagToClassTagPointer(classTag);
    if (pClassTag && *tagPtr == 0) {
        *tagPtr = pointerToTag(pClassTag->createStartTag());
    }

    return JVMTI_ITERATION_CONTINUE;
}

jvmtiError walkHeapFromObjects(jvmtiEnv *jvmti,
                               const std::vector<jobject> &objects,
                               const std::chrono::steady_clock::time_point &finishTime) {
    jvmtiError err = JVMTI_ERROR_NONE;
    if (!objects.empty()) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&spreadInfo);
        int heapWalksCnt = 0;
        for (auto &object : objects) {
            if (finishTime < std::chrono::steady_clock::now()) {
                return MEMORY_AGENT_TIMEOUT_ERROR;
            }

            jlong tag;
            err = jvmti->GetTag(object, &tag);
            if (err != JVMTI_ERROR_NONE) return err;

            if (tagsWithNewInfo.find(tag) != tagsWithNewInfo.end()) {
                tagsWithNewInfo.erase(tag);
                err = jvmti->FollowReferences(0, nullptr, object, &cb, nullptr);
                if (err != JVMTI_ERROR_NONE) return err;
                debug(std::to_string(tagsWithNewInfo.size()).c_str());
                heapWalksCnt++;
            }
        }
        debug(std::string("Total heap walks: " + std::to_string(heapWalksCnt)).c_str());
    }

    return err;
}
