// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_MERGED_H
#define MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_MERGED_H

#include <vector>
#include "../memory_agent_action.h"

class RetainedSizeByObjectsMergedAction : public MemoryAgentAction<jlongArray, jobjectArray> {
public:
    RetainedSizeByObjectsMergedAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jlongArray executeOperation(jobjectArray objects) override;
    jvmtiError cleanHeap() override;

    jvmtiError estimateObjectsSize(std::vector<jobject> &objects, jlong &retainedSize);

    jvmtiError traverseHeapForTheFirstTime(std::vector<jobject> &objects);

    jvmtiError traverseHeapFromStartObject(std::vector<jobject> &objects);
    jvmtiError countRetainedSize(jlong &retainedSize);

};


#endif //MEMORY_AGENT_RETAINED_SIZE_BY_OBJECTS_MERGED_H
