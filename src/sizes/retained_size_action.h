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

        if (sizesTagBalance != 0) {
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
        debug("getTag objects of classes");
        jvmtiError err = createTagsForClasses(classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        return this->IterateThroughHeap(0, nullptr, tagObjectOfTaggedClass, nullptr);
    }

    jvmtiError tagHeap() {
        jvmtiError err = this->FollowReferences(0, nullptr, nullptr, getTagsWithNewInfo, &this->finishTime, "find objects with new info");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        std::vector<std::pair<jobject, jlong>> objectsAndTags;
        debug("collect objects with new info");
        err = getObjectsByTags(this->jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)}, objectsAndTags);
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        err = this->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, retagStartObjects, nullptr, "retag start objects");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        err = this->FollowReferences(0, nullptr, nullptr, visitReference, &this->finishTime, "getTag heap");
        if (err != JVMTI_ERROR_NONE) return err;
        if (this->shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        return walkHeapFromObjects(this->jvmti, objectsAndTags, this->finishTime);
    }
};

#endif //MEMORY_AGENT_RETAINED_SIZE_ACTION_H
