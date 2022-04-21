// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H
#define MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H

#include <jni.h>
#include <jvmti.h>
#include "../memory_agent_action.h"

// Forward declaration
class SizesViaDominatorTreeHeapDumpInfo;

template<typename RESULT_TYPE, typename... ARGS_TYPES>
class RetainedSizesAction : public MemoryAgentAction<RESULT_TYPE, ARGS_TYPES...> {
public:
    RetainedSizesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

protected:
    jvmtiError calculateRetainedSizes(jobjectArray objects, std::vector<jlong> &retainedSizes,
                                      SizesViaDominatorTreeHeapDumpInfo &info);
};

class RetainedSizesViaDominatorTreeAction : public RetainedSizesAction<jobjectArray, jobjectArray> {
public:
    RetainedSizesViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobjectArray objects) override;
    jvmtiError cleanHeap() override;

    jobjectArray constructResultObject(jobjectArray objects, const std::vector<jlong> &retainedSizes,
                                       const SizesViaDominatorTreeHeapDumpInfo &info);
};

class RetainedSizesByClassViaDominatorTreeAction : public RetainedSizesAction<jobjectArray, jobject, jlong> {
public:
    RetainedSizesByClassViaDominatorTreeAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobject classRef, jlong objectsLimit) override;
    jvmtiError cleanHeap() override;

    jobjectArray constructResultObject(const std::vector<jobject> &objectsOfClass, const std::vector<jlong> &retainedSizes,
                                       const SizesViaDominatorTreeHeapDumpInfo &info, jlong objectsLimit);
};

#endif //MEMORY_AGENT_RETAINED_SIZE_VIA_DOMINATOR_TREE_ACTION_H
