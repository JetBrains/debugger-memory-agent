// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_SIZES_TAGS_H
#define MEMORY_AGENT_SIZES_TAGS_H

#include "jni.h"
#include "tag_info_array.h"

static jlong sizesTagBalance = 0;

class Tag {
public:
    const static Tag EmptyTag;
    const static Tag TagWithNewInfo;
    const static Tag HeldObjectTag;

protected:
    Tag(query_size_t index, uint8_t state);

    Tag(const Tag &copyTag);

    Tag(const Tag &referree, const Tag &referrer);

    explicit Tag(TagInfoArray &&array);

    Tag();

public:
    ~Tag();

    static Tag *create(query_size_t index, uint8_t state);
    static Tag *create(const Tag &referree, const Tag &referrer);

    virtual Tag *share();

    virtual query_size_t getId() const { return 0; }

    void ref();
    void unref();

    void visitFromUntaggedReferrer() const;

private:
    static Tag *copyWithoutStartMarks(const Tag &tag);

public:
    bool isStartTag;
    bool alreadyReferred;
    TagInfoArray array;
    uint32_t refCount;
};


class ClassTag : public Tag {
private:
    explicit ClassTag(query_size_t id);

    explicit ClassTag(query_size_t index, uint8_t state, query_size_t id);

public:
    query_size_t getId() const override { return id; }

    static Tag *create(query_size_t index);
    static Tag *create(query_size_t index, uint8_t state, query_size_t id);

private:
    query_size_t id;
};


Tag *tagToPointer(jlong tag);

bool isEmptyTag(jlong tag);

bool isTagWithNewInfo(jlong tag);

Tag *merge(jlong referreeTag, jlong referrerTag);

bool shouldMerge(jlong referree, jlong referrer);

#endif //MEMORY_AGENT_SIZES_TAGS_H
