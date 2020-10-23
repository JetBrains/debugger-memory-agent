// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_PATHS_TO_CLOSEST_GC_ROOTS_H
#define MEMORY_AGENT_PATHS_TO_CLOSEST_GC_ROOTS_H

#include "../timed_action.h"
#include "roots_tags.h"

#define MEMORY_AGENT_TRUNCATE_REFERENCE static_cast<jvmtiHeapReferenceKind>(42)

class PathsToClosestGcRootsAction : public MemoryAgentTimedAction<jobjectArray, jobject, jint, jint> {
public:
    PathsToClosestGcRootsAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName);

private:
    jobjectArray executeOperation(jobject object, jint pathsNumber, jint objectsNumber) override;
    jvmtiError cleanHeap() override;

    GcTag *createTags(jobject target);

    jobjectArray collectPathsToClosestGcRoots(jlong start, jint number, jint objectsNumber);
};

void setTagsForReferences(JNIEnv *env, jvmtiEnv *jvmti, jlong tag);

#endif //MEMORY_AGENT_PATHS_TO_CLOSEST_GC_ROOTS_H
