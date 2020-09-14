// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "roots_state.h"

State::State() : state(0) {

}

State::State(bool isAlreadyVisited, bool isWeakSoftReachable) : state(0) {
    setAlreadyVisited(isAlreadyVisited);
    setWeakSoftReachable(isWeakSoftReachable);
}

void State::setAlreadyVisited(bool value) {
    setAttribute(value, 0);
}

void State::setWeakSoftReachable(bool value) {
    setAttribute(value, 1u);
}

bool State::isAlreadyVisited() const {
    return checkAttribute(0u);
}

bool State::isWeakSoftReachable() const {
    return checkAttribute(1u);
}

void State::updateWeakSoftReachableValue(const State &referrerState) {
    if (isWeakSoftReachable()) {
        if (!referrerState.isWeakSoftReachable()) {
            setWeakSoftReachable(false);
        }
    } else {
        if (referrerState.isWeakSoftReachable() && !isAlreadyVisited()) {
            setWeakSoftReachable(true);
        }
    }
}

void State::setAttribute(bool value, uint8_t offset) {
    state = value ? state | (1u << offset) : state & ~(1u << offset);
}

bool State::checkAttribute(uint8_t offset) const {
    return (state & (1u << offset)) != 0u;
}
