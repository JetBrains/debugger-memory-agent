// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_OBJECTS_SIZE_H
#define MEMORY_AGENT_OBJECTS_SIZE_H

#include <jni.h>
#include <jvmti.h>

jobjectArray estimateObjectSize(JNIEnv *env, jvmtiEnv *jvmti, jobject object);

jlongArray estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray objects);

jlongArray getRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray);

jobjectArray getShallowAndRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray);

#endif //MEMORY_AGENT_OBJECTS_SIZE_H
