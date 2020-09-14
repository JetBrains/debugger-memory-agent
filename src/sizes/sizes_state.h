// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SIZES_STATE_H
#define MEMORY_AGENT_SIZES_STATE_H

#include <cstdint>

bool isStartObject(uint8_t state);

bool isInSubtree(uint8_t state);

bool isReachableOutside(uint8_t state);

bool isAlreadyVisited(uint8_t state);

uint8_t createState(bool isStartObject, bool isInSubtree, bool isReachableOutside, bool isAlreadyVisited = true);

bool isRetained(uint8_t state);

uint8_t updateState(uint8_t currentState, uint8_t referrerState);

uint8_t asVisitedFromUntagged(uint8_t state);

#endif //MEMORY_AGENT_SIZES_STATE_H
