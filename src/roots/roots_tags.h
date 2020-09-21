// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_ROOTS_TAGS_H
#define MEMORY_AGENT_ROOTS_TAGS_H

#include "jni.h"
#include "roots_state.h"
#include "infos.h"

static jlong rootsTagBalance = 0;

class GcTag {
public:
    const static GcTag WeakSoftReferenceTag;

public:
    GcTag();
    explicit GcTag(bool isWeakSoftReachable);

    ~GcTag();

    static GcTag *create(jlong classTag);
    static GcTag *create();

    static GcTag *pointerToGcTag(jlong tagPtr);

    static void cleanTag(jlong tag);

    bool isWeakSoftReachable() const;

    void setVisited();

    void updateState(const GcTag *referrer);

public:
    std::vector<ReferenceInfo *> backRefs;

private:
    State state;
};

#endif //MEMORY_AGENT_ROOTS_TAGS_H
