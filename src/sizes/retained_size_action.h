// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_ACTION_H
#define MEMORY_AGENT_RETAINED_SIZE_ACTION_H

#include <vector>
#include <string>
#include <unordered_set>
#include "../memory_agent_action.h"
#include "sizes_tags.h"

extern std::unordered_set<jlong> tagsWithNewInfo;

jint JNICALL getTagsWithNewInfo     (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL visitReference         (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL spreadInfo             (jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                     jlong referrerClassTag, jlong size, jlong *tagPtr,
                                     jlong *referrerTagPtr, jint length, void *userData);

jint JNICALL clearTag               (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jint JNICALL retagStartObjects      (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jint JNICALL tagObjectOfTaggedClass (jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

jvmtiError walkHeapFromObjects      (jvmtiEnv *jvmti, const std::vector<jobject> &objects, const CancellationChecker &cancellationChecker);

template<typename RESULT_TYPE>
class RetainedSizeAction : public MemoryAgentAction<RESULT_TYPE, jobjectArray> {
protected:
    RetainedSizeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object) : MemoryAgentAction<RESULT_TYPE, jobjectArray>(env, jvmti, object) {

    }

    virtual RESULT_TYPE executeOperation(jobjectArray) = 0;

    jvmtiError cleanHeap() final {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = clearTag;
        jvmtiError err =  this->jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);

        if (sizesTagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }

    jvmtiError createTagsForClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
        for (jsize i = 0; i < this->env->GetArrayLength(classesArray); i++) {
            jobject classObject = this->env->GetObjectArrayElement(classesArray, i);
            jvmtiError err = tagClassAndItsInheritors(env, jvmti, classObject, [i](jlong oldTag) -> jlong {
                ClassTag *classTag = tagToClassTagPointer(oldTag);
                if (classTag != nullptr) {
                    classTag->ids.push_back(i);
                } else {
                    return pointerToTag(ClassTag::create(static_cast<query_size_t>(i)));
                }

                return 0;
            });
            if (err != JVMTI_ERROR_NONE) return err;
        }

        return JVMTI_ERROR_NONE;
    }

    jvmtiError tagObjectsOfClasses(jobjectArray classesArray) {
        debug("tag objects of classes");
        jvmtiError err = createTagsForClasses(this->env, this->jvmti, classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        return this->IterateThroughHeap(0, nullptr, tagObjectOfTaggedClass, nullptr);
    }

    jvmtiError tagHeap() {
        jvmtiError err = this->FollowReferences(0, nullptr, nullptr, getTagsWithNewInfo, nullptr, "find objects with new info");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

        std::vector<jobject> objects;
        debug("collect objects with new info");
        err = getObjectsByTags(this->jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)}, objects);
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

        err = this->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, retagStartObjects, nullptr, "retag start objects");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

        err = this->FollowReferences(0, nullptr, nullptr, visitReference, nullptr, "tag heap");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

        return walkHeapFromObjects(this->jvmti, objects, *dynamic_cast<CancellationChecker *>(this));
    }
};

#endif //MEMORY_AGENT_RETAINED_SIZE_ACTION_H
