// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H
#define MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H

#include "../memory_agent_action.h"

class RetainedSizeAndHeldObjectsAction : public MemoryAgentAction<jobjectArray, jobject> {
public:
    RetainedSizeAndHeldObjectsAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobject object) override;
    jvmtiError cleanHeap() override;

    jvmtiError estimateObjectSize(jobject &object, jlong &retainedSize, std::vector<jobject> &heldObjects);

    jobjectArray createResultObject(jlong retainedSize, jlong shallowSize, const std::vector<jobject> &heldObjects);

    jvmtiError traverseHeapForTheFirstTime(jobject &object);

    jvmtiError traverseHeapFromStartObjectAndCountRetainedSize(jobject &object, jlong &retainedSize);
};

#endif //MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H
