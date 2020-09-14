// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_ROOTS_STATE_H
#define MEMORY_AGENT_ROOTS_STATE_H

#include <cstdint>

class State {
public:
    State();
    State(bool isAlreadyVisited, bool isWeakSoftReachable);

    void setAlreadyVisited(bool value);

    void setWeakSoftReachable(bool value);

    bool isAlreadyVisited() const;

    bool isWeakSoftReachable() const;

    void updateWeakSoftReachableValue(const State &referrerState);

private:
    void setAttribute(bool value, uint8_t offset);

    bool checkAttribute(uint8_t offset) const;

private:
    uint8_t state;
};

#endif //MEMORY_AGENT_ROOTS_STATE_H
