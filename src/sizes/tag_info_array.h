// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_TAG_INFO_ARRAY_H
#define MEMORY_AGENT_TAG_INFO_ARRAY_H

#include <cstdint>
#include <memory>
#include "sizes_state.h"

using query_size_t = uint16_t;

class TagInfoArray {
public:
    class TagInfo {
    public:
        TagInfo();
        TagInfo(query_size_t index, uint8_t state);

    public:
        query_size_t index;
        uint8_t state;
    };

public:
    TagInfoArray();

    TagInfoArray(query_size_t index, uint8_t state);

    TagInfoArray(const TagInfoArray &copyArray);

    TagInfoArray(TagInfoArray &&copyArray) noexcept;

    TagInfoArray(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray);

    explicit TagInfoArray(query_size_t size);

    void extend(const TagInfoArray &infoArray);

    query_size_t getSize() const { return size; }

    TagInfo &operator[](query_size_t i) const { return arrayPtr[i]; }

private:
    static query_size_t getNewArraySize(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray);

private:
    std::unique_ptr<TagInfo[]> arrayPtr;
    query_size_t size;
};

#endif //MEMORY_AGENT_TAG_INFO_ARRAY_H
