// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_RETAINED_SIZE_BY_CLASSES_H
#define MEMORY_AGENT_RETAINED_SIZE_BY_CLASSES_H

#include "../memory_agent_action.h"
#include "retained_size_action.h"

class RetainedSizeByClassesAction : public RetainedSizeAction<jlongArray> {
public:
    RetainedSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jlongArray executeOperation(jobjectArray classesArray) override;
    jvmtiError getRetainedSizeByClasses(jobjectArray classesArray, std::vector<jlong> &result);
};

class RetainedAndShallowSizeByClassesAction : public RetainedSizeAction<jobjectArray> {
public:
    RetainedAndShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

private:
    jobjectArray executeOperation(jobjectArray classesArray) override;
    jvmtiError getShallowAndRetainedSizeByClasses(jobjectArray classesArray, std::vector<jlong> &shallowSizes,
                                                  std::vector<jlong> &retainedSizes);
};

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

#endif //MEMORY_AGENT_RETAINED_SIZE_BY_CLASSES_H
