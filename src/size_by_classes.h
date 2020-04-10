// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SIZE_BY_CLASSES_H
#define MEMORY_AGENT_SIZE_BY_CLASSES_H


#include <jni.h>
#include <jvmti.h>

jlongArray getSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray);

void tagClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray, bool setTagToZero = false);

#endif //MEMORY_AGENT_SIZE_BY_CLASSES_H
