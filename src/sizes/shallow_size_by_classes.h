// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H
#define MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H

#include "../memory_agent_action.h"

class ShallowSizeByClassesAction : public MemoryAgentAction<jlongArray, jobjectArray> {
public:
    ShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jlongArray executeOperation(jobjectArray classesArray) override;
    jvmtiError cleanHeap() override;

    void tagClasses(jobjectArray classesArray);
};


#endif //MEMORY_AGENT_SHALLOW_SIZE_BY_CLASSES_H
