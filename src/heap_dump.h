// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef NATIVE_MEMORY_AGENT_HEAP_DUMP_H
#define NATIVE_MEMORY_AGENT_HEAP_DUMP_H

#include <jni.h>
#include "types.h"

jobjectArray fetchHeapDump(JNIEnv *jni, jvmtiEnv *jvmti);

#endif //NATIVE_MEMORY_AGENT_HEAP_DUMP_H
