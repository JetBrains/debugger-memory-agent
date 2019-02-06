// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef NATIVE_MEMORY_AGENT_TYPES_H
#define NATIVE_MEMORY_AGENT_TYPES_H

#include <vector>
#include <jvmti.h>
#include <iostream>

typedef struct {
    jvmtiEnv *jvmti;
} GlobalAgentData;

#endif //NATIVE_MEMORY_AGENT_TYPES_H
