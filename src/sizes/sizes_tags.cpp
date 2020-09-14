// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "../utils.h"
#include "sizes_tags.h"

const Tag Tag::EmptyTag;
const Tag Tag::TagWithNewInfo;
const Tag Tag::HeldObjectTag;

Tag::Tag(query_size_t index, uint8_t state) :
    array(index, state), isStartTag(true),
    refCount(1), alreadyReferred(false) {

}

Tag::Tag(const Tag &copyTag) :
    array(copyTag.array), refCount(1),
    isStartTag(copyTag.isStartTag), alreadyReferred(copyTag.alreadyReferred) {

}

Tag::Tag(const Tag &referree, const Tag &referrer) :
    array(referree.array, referrer.array), refCount(1),
    isStartTag(referree.isStartTag), alreadyReferred(referree.alreadyReferred) {

}

Tag::Tag(TagInfoArray &&array) :
    array(std::move(array)), refCount(1),
    isStartTag(false), alreadyReferred(false) {

}

Tag::Tag() : array(), refCount(1), isStartTag(false), alreadyReferred(false) {

}

Tag::~Tag() {
    --tagBalance;
}

Tag *Tag::create(query_size_t index, uint8_t state) {
    ++tagBalance;
    return new Tag(index, state);
}

Tag *Tag::create(const Tag &referree, const Tag &referrer) {
    ++tagBalance;
    return new Tag(referree, referrer);
}

Tag *Tag::copyWithoutStartMarks(const Tag &tag) {
    TagInfoArray result(tag.array.getSize());
    for (query_size_t i = 0; i < result.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag.array[i];
        uint8_t state = updateState(createState(false, false, false, true), info.state);
        result[i] = TagInfoArray::TagInfo(info.index, state);
    }

    ++tagBalance;
    return new Tag(std::move(result));
}

Tag *Tag::share() {
    if (isStartTag) {
        return copyWithoutStartMarks(*this);
    }
    ref();
    return this;
}

void Tag::ref() {
    ++refCount;
}

void Tag::unref() {
    if (this == &EmptyTag || this == &HeldObjectTag || this == &TagWithNewInfo) {
        return;
    }

    if (--refCount == 0) {
        delete this;
    }
}

void Tag::visitFromUntaggedReferrer() const {
    for (query_size_t i = 0; i < array.getSize(); i++) {
        array[i].state = asVisitedFromUntagged(array[i].state);
    }
}

ClassTag::ClassTag(query_size_t id) : Tag(), id(id) {

}

ClassTag::ClassTag(query_size_t index, uint8_t state, query_size_t id) : Tag(index, state), id(id) {

}

Tag *ClassTag::create(query_size_t index) {
    ++tagBalance;
    return new ClassTag(index);
}

Tag *ClassTag::create(query_size_t index, uint8_t state, query_size_t id) {
    ++tagBalance;
    return new ClassTag(index, state, id);
}

Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}

bool isEmptyTag(jlong tag) {
    return tag == pointerToTag(&Tag::EmptyTag);
}

bool isTagWithNewInfo(jlong tag) {
    return tag == pointerToTag(&Tag::TagWithNewInfo);
}

Tag *merge(jlong referreeTag, jlong referrerTag) {
    Tag *result = Tag::create(*tagToPointer(referreeTag), *tagToPointer(referrerTag));
    tagToPointer(referreeTag)->unref();

    return result;
}

static bool shouldMerge(const Tag &referree, const Tag &referrer) {
    if (referrer.array.getSize() > referree.array.getSize()) {
        return true;
    }

    query_size_t i = 0;
    query_size_t j = 0;
    while (i < referree.array.getSize() && j < referrer.array.getSize()) {
        if (referree.array[i].index == referrer.array[j].index) {
            if (referree.array[i].state == referrer.array[j].state) {
                i++;
                j++;
            } else {
                return true;
            }
        } else if (referree.array[i].index < referrer.array[j].index) {
            i++;
        } else {
            return true;
        }
    }

    if (j < referrer.array.getSize()) {
        return true;
    }
    return false;
}

bool shouldMerge(jlong referree, jlong referrer) {
    return shouldMerge(*tagToPointer(referree), *tagToPointer(referrer));
}
