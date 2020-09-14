// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_H
#define MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_H

#include <vector>
#include "../timed_action.h"

class RetainedSizeByObjectsAction : public MemoryAgentTimedAction<jlongArray, jobjectArray> {
public:
    RetainedSizeByObjectsAction(JNIEnv *env, jvmtiEnv *jvmti);

private:
    jlongArray executeOperation(jobjectArray objects) override;
    jvmtiError cleanHeap() override;

    jvmtiError estimateObjectsSizes(const std::__1::vector<jobject> &objects, std::__1::vector<jlong> &result);

    jvmtiError tagHeap(const std::vector<jobject> &objects);

    jvmtiError retagStartObjects(const std::vector<jobject> &objects);

    jvmtiError createTagsForObjects(const std::vector<jobject> &objects);

    jvmtiError createTagForObject(jobject object, size_t index);

    jvmtiError calculateRetainedSizes(const std::vector<jobject> &objects, std::vector<jlong> &result);
};


#endif //MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_H
