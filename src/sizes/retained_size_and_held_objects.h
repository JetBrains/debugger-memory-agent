// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H
#define MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H

#include "../timed_action.h"


class RetainedSizeAndHeldObjectsAction : public MemoryAgentTimedAction<jobjectArray, jobject> {
public:
    RetainedSizeAndHeldObjectsAction(JNIEnv *env, jvmtiEnv *jvmti);

private:
    jobjectArray executeOperation(jobject object) override;
    jvmtiError cleanHeap() override;

    jobjectArray createResultObject(jlong retainedSize, const std::vector<jobject> &heldObjects);

    jvmtiError estimateObjectSize(jobject &object, jlong &retainedSize, std::vector<jobject> &heldObjects);

    jvmtiError tagHeap(jobject object);

    jvmtiError retagStartObject(jobject object);
};

#endif //MEMORY_AGENT_RETAINED_SIZE_AND_HELD_OBJECTS_H
