// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef NATIVE_MEMORY_AGENT_GC_ROOTS_H
#define NATIVE_MEMORY_AGENT_GC_ROOTS_H

#include <jni.h>

jobjectArray findGcRoots(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object, jint limit);

jobjectArray findPathToClosestGcRoot(JNIEnv *jni, jvmtiEnv *jvmti, jclass thisClass, jobject object);

#endif //NATIVE_MEMORY_AGENT_GC_ROOTS_H
