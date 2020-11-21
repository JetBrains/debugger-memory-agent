// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H
#define MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H

#include "../timed_action.h"

class ShallowSizeByClassesAction : public MemoryAgentTimedAction<jlongArray, jobjectArray> {
public:
    ShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName, jlong duration);

private:
    jlongArray executeOperation(jobjectArray classesArray) override;
    jvmtiError cleanHeap() override;

    void tagClasses(jobjectArray classesArray);
};


#endif //MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H
