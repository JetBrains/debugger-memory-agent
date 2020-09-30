// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "tag_info_array.h"

TagInfoArray::TagInfo::TagInfo() : index(0), state(0) {

}

TagInfoArray::TagInfo::TagInfo(query_size_t index, uint8_t state) : index(index), state(state) {

}

TagInfoArray::TagInfoArray() : arrayPtr(nullptr), size(0) {

}

TagInfoArray::TagInfoArray(query_size_t size) : size(size), arrayPtr(new TagInfo[size]) {

}

TagInfoArray::TagInfoArray(query_size_t index, uint8_t state) : size(1), arrayPtr(new TagInfo[1]{TagInfo(index, state)}) {

}

TagInfoArray::TagInfoArray(const TagInfoArray &copyArray) : size(copyArray.size), arrayPtr(new TagInfo[copyArray.size]) {
    for (query_size_t i = 0; i < size; i++) {
        arrayPtr[i] = copyArray[i];
    }
}

TagInfoArray::TagInfoArray(TagInfoArray &&copyArray) noexcept : size(copyArray.size), arrayPtr(copyArray.arrayPtr.release()) {

}

TagInfoArray::TagInfoArray(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray) {
    size = getNewArraySize(referreeArray, referrerArray);
    arrayPtr = std::unique_ptr<TagInfo[]>(new TagInfo[size]);
    bool alreadyVisited = referreeArray.size == 0 || isAlreadyVisited(referreeArray.arrayPtr[0].state);
    uint8_t newState;
    query_size_t i = 0;
    query_size_t j = 0;
    query_size_t k = 0;
    while (i < referreeArray.size && j < referrerArray.size) {
        if (referreeArray[i].index == referrerArray[j].index) {
            newState = updateState(referreeArray[i].state, referrerArray[j].state);
            arrayPtr[k++] = TagInfo(referreeArray[i].index, newState);
            i++;
            j++;
        } else if (referreeArray[i].index < referrerArray[j].index) {
            newState = updateState(referreeArray[i].state, createState(false, false, true));
            arrayPtr[k++] = TagInfo(referreeArray[i].index, newState);
            i++;
        } else {
            newState = updateState(createState(false, false, alreadyVisited, true), referrerArray[j].state);
            arrayPtr[k++] = TagInfo(referrerArray[j].index, newState);
            j++;
        }
    }

    while (j < referrerArray.size) {
        newState = updateState(createState(false, false, alreadyVisited), referrerArray[j].state);
        arrayPtr[k++] = TagInfo(referrerArray[j].index, newState);
        j++;
    }

    while (i < referreeArray.size) {
        newState = updateState(referreeArray[i].state, createState(false, false, true));
        arrayPtr[k++] = TagInfo(referreeArray[i].index, newState);
        i++;
    }
}

void TagInfoArray::extend(const TagInfoArray &infoArray) {
    auto *newArray = new TagInfo[size + infoArray.size];
    for (query_size_t i = 0; i < size; i++) {
        newArray[i] = arrayPtr[i];
    }

    for (query_size_t i = 0; i < infoArray.size; i++) {
        newArray[size + i] = infoArray[i];
    }

    size += infoArray.size;
    arrayPtr = std::unique_ptr<TagInfo[]>(newArray);
}

query_size_t TagInfoArray::getNewArraySize(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray) {
    query_size_t i = 0;
    query_size_t j = 0;
    query_size_t size = 0;
    while (i < referreeArray.size && j < referrerArray.size) {
        if (referreeArray[i].index == referrerArray[j].index) {
            i++;
            j++;
        } else if (referreeArray[i].index < referrerArray[j].index) {
            i++;
        } else {
            j++;
        }
        size++;
    }

    if (i < referreeArray.size) {
        size += referreeArray.size - i;
    } else if (j < referrerArray.size) {
        size += referrerArray.size - j;
    }

    return size;
}
