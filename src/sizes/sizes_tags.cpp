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

Tag::Tag(TagInfoArray &&array, bool isStartTag) :
    array(std::move(array)), refCount(1),
    isStartTag(isStartTag), alreadyReferred(false) {

}

Tag::Tag() : array(), refCount(1), isStartTag(false), alreadyReferred(false) {

}

Tag *Tag::create(query_size_t index, uint8_t state) {
    ++sizesTagBalance;
    return new Tag(index, state);
}

Tag *Tag::create(const Tag &referree, const Tag &referrer) {
    ++sizesTagBalance;
    return new Tag(referree, referrer);
}

Tag *Tag::copyWithoutStartMarks(const Tag &tag) {
    TagInfoArray result(tag.array.getSize());
    for (query_size_t i = 0; i < result.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag.array[i];
        uint8_t state = updateState(createState(false, false, false, true), info.state);
        result[i] = TagInfoArray::TagInfo(info.index, state);
    }

    return create(std::move(result), false);
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

Tag *Tag::create(TagInfoArray &&array, bool isStartTag) {
    ++sizesTagBalance;
    return new Tag(std::move(array), isStartTag);
}

ClassTag::ClassTag(query_size_t id) : Tag() {
    ids.push_back(id);
}

Tag *ClassTag::create(query_size_t index) {
    ++sizesTagBalance;
    return new ClassTag(index);
}

Tag *ClassTag::createStartTag() {
    TagInfoArray array(ids.size());
    for (int i = 0; i < ids.size(); i++) {
        array[i] = TagInfoArray::TagInfo(ids[i],createState(true, true, false, false));
    }

    return Tag::create(std::move(array), true);
}

Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}

ClassTag *tagToClassTagPointer(jlong tag) {
    return dynamic_cast<ClassTag *>(tagToPointer(tag));
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
