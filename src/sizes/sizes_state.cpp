// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "sizes_state.h"

bool isStartObject(uint8_t state) {
    return (state & 1u) != 0u;
}

bool isInSubtree(uint8_t state) {
    return (state & (1u << 1u)) != 0u;
}

bool isReachableOutside(uint8_t state) {
    return (state & (1u << 2u)) != 0u;
}

bool isAlreadyVisited(uint8_t state) {
    return (state & (1u << 3u)) != 0u;
}

uint8_t createState(bool isStartObject, bool isInSubtree, bool isReachableOutside, bool isAlreadyVisited) {
    uint8_t state = 0;
    if (isStartObject) {
        state |= 1u;
    }
    if (isInSubtree) {
        state |= 1u << 1u;
    }
    if (isReachableOutside) {
        state |= 1u << 2u;
    }
    if (isAlreadyVisited) {
        state |= 1u << 3u;
    }
    return state;
}

bool isRetained(uint8_t state) {
    return isStartObject(state) || (isInSubtree(state) && !isReachableOutside(state));
}

uint8_t updateState(uint8_t currentState, uint8_t referrerState) {
    return createState(
            isStartObject(currentState),
            isInSubtree(currentState) || isInSubtree(referrerState),
            isReachableOutside(currentState) ||
            (!isStartObject(referrerState) && isReachableOutside(referrerState)),
            true
    );
}

uint8_t asVisitedFromUntagged(uint8_t state) {
    state |= 1u << 2u;
    state |= 1u << 3u;
    return state;
}
