// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_ACTION_H
#define MEMORY_AGENT_RETAINED_SIZE_ACTION_H

#include <vector>
#include <string>
#include "../timed_action.h"
#include "sizes_callbacks.h"
#include "sizes_tags.h"
#include "../utils.h"

template<typename RESULT_TYPE>
class RetainedSizeAction : public MemoryAgentTimedAction<RESULT_TYPE, jobjectArray> {
protected:
    RetainedSizeAction(JNIEnv *env, jvmtiEnv *jvmti) : MemoryAgentTimedAction<RESULT_TYPE, jobjectArray>(env, jvmti) {

    }

    virtual RESULT_TYPE executeOperation(jobjectArray) = 0;

    jvmtiError cleanHeap() final {
        jvmtiError err = this->IterateThroughHeap(0, nullptr, clearTag, nullptr, "clear tags");

        if (tagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }

    jvmtiError createTagsForClasses(jobjectArray classesArray) {
        for (jsize i = 0; i < this->env->GetArrayLength(classesArray); i++) {
            jobject classObject = this->env->GetObjectArrayElement(classesArray, i);
            Tag *tag = ClassTag::create(static_cast<query_size_t>(i + 1));
            jvmtiError err = this->jvmti->SetTag(classObject, pointerToTag(tag));
            if (err != JVMTI_ERROR_NONE) return err;
        }

        return JVMTI_ERROR_NONE;
    }

    jvmtiError tagObjectsOfClasses(jobjectArray classesArray) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&tagObjectOfTaggedClass);

        debug("getTag objects of classes");
        jvmtiError err = createTagsForClasses(classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        return this->jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);
    }

    jvmtiError tagHeap() {
        jvmtiError err = this->FollowReferences(0, nullptr, nullptr, getTagsWithNewInfo, nullptr, "find objects with new info");
        if (err != JVMTI_ERROR_NONE) return err;

        std::vector<std::pair<jobject, jlong>> objectsAndTags;
        debug("collect objects with new info");
        err = getObjectsByTags(this->jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)}, objectsAndTags);
        if (err != JVMTI_ERROR_NONE) return err;

        err = this->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, retagStartObjects, nullptr, "retag start objects");
        if (err != JVMTI_ERROR_NONE) return err;

        err = this->FollowReferences(0, nullptr, nullptr, visitReference, nullptr, "getTag heap");
        if (err != JVMTI_ERROR_NONE) return err;

        return walkHeapFromObjects(this->jvmti, objectsAndTags);
    }
};

#endif //MEMORY_AGENT_RETAINED_SIZE_ACTION_H
