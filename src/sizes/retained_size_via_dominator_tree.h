// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H
#define MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H

#include <jni.h>
#include <jvmti.h>
#include "../memory_agent_action.h"

class RetainedSizeViaDominatorTreeAction : public MemoryAgentAction<jobjectArray, jobjectArray> {
public:
    RetainedSizeViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobjectArray objects) override;
    jvmtiError cleanHeap() override;
};

class RetainedSizeByClassViaDominatorTreeAction : public MemoryAgentAction<jobjectArray, jobject, jlong> {
public:
    RetainedSizeByClassViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobject classRef, jlong objectsLimit) override;
    jvmtiError cleanHeap() override;
};

#endif //MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H
