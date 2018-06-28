//
// Created by Vitaliy.Bibaev on 28-Jun-18.
//

#ifndef NATIVE_MEMORY_AGENT_TYPES_H
#define NATIVE_MEMORY_AGENT_TYPES_H
#include <vector>

typedef struct {
    jvmtiEnv *jvmti;
} GlobalAgentData;

typedef struct Tag {
    bool in_subtree;
    bool reachable_outside;
    bool start_object;
} Tag;

struct PathNodeTag {
    bool target;
    int index;
    std::vector<PathNodeTag*> *prev;
};

#endif //NATIVE_MEMORY_AGENT_TYPES_H
