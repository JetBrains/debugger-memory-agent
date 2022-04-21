// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_DOMINATOR_TREE_H
#define MEMORY_AGENT_DOMINATOR_TREE_H

#include <vector>
#include <jni.h>

std::vector<jlong> calculateRetainedSizes(const std::vector<std::vector<jlong>> &graph, const std::vector<jlong> &sizes);

#endif //MEMORY_AGENT_DOMINATOR_TREE_H
