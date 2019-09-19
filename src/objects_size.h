// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_OBJECTS_SIZE_H
#define MEMORY_AGENT_OBJECTS_SIZE_H

#include <jni.h>
#include <jvmti.h>
#include <unordered_map>

jlongArray estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray objects);

jobjectArray estimateObjectsSizesByPluginClassLoaders(JNIEnv *env, jvmtiEnv *jvmti);

#endif //MEMORY_AGENT_OBJECTS_SIZE_H
