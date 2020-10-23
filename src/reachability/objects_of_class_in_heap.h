// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_OBJECTS_OF_CLASS_IN_HEAP_H
#define MEMORY_AGENT_OBJECTS_OF_CLASS_IN_HEAP_H

#include "../timed_action.h"

class GetFirstReachableObjectOfClassAction : public MemoryAgentTimedAction<jobject, jobject, jobject> {
public:
    GetFirstReachableObjectOfClassAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName);

private:
    jobject executeOperation(jobject startObject, jobject classObject) override;
    jvmtiError cleanHeap() override;
};

class GetAllReachableObjectsOfClassAction : public MemoryAgentTimedAction<jobjectArray, jobject, jobject> {
public:
    GetAllReachableObjectsOfClassAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName);

private:
    jobjectArray executeOperation(jobject startObject, jobject classObject) override;
    jvmtiError cleanHeap() override;
};

#endif //MEMORY_AGENT_OBJECTS_OF_CLASS_IN_HEAP_H
