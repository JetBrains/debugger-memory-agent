// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include <memory>
#include "objects_size.h"
#include "utils.h"
#include "log.h"

using query_size_t = uint16_t;

namespace {
    jlong tagBalance = 0;

    bool isStartObject(uint8_t state) {
        return (state & 1u) != 0u;
    }

    bool isInSubtree(uint8_t state) {
        return (state & (1u << 1u)) != 0u;
    }

    bool isReachableOutside(uint8_t state) {
        return (state & (1u << 2u)) != 0u;
    }

    bool isAlreadyVisited(uint8_t state) {
        return (state & (1u << 3u)) != 0u;
    }

    uint8_t createState(bool isStartObject, bool isInSubtree, bool isReachableOutside, bool isAlreadyVisited = true) {
        uint8_t state = 0;
        if (isStartObject) {
            state |= 1u;
        }
        if (isInSubtree) {
            state |= 1u << 1u;
        }
        if (isReachableOutside) {
            state |= 1u << 2u;
        }
        if (isAlreadyVisited) {
            state |= 1u << 3u;
        }
        return state;
    }

    bool isRetained(uint8_t state) {
        return isStartObject(state) || (isInSubtree(state) && !isReachableOutside(state));
    }

    uint8_t updateState(uint8_t currentState, uint8_t referrerState) {
        return createState(
                isStartObject(currentState),
                isInSubtree(currentState) || isInSubtree(referrerState),
                isReachableOutside(currentState) ||
                (!isStartObject(referrerState) && isReachableOutside(referrerState)),
                true
        );
    }

    uint8_t asVisitedFromUntagged(uint8_t state) {
        state |= 1u << 2u;
        state |= 1u << 3u;
        return state;
    }

    struct TagInfo {
        TagInfo() : index(0), state(0) {

        }

        TagInfo(query_size_t index, uint8_t state) :
                index(index), state(state) {

        }

        query_size_t index;
        uint8_t state;
    };

    class TagInfoArray {
    public:
        TagInfoArray() : arrayPtr(nullptr), size(0) {

        }

        explicit TagInfoArray(query_size_t size) : size(size), arrayPtr(new TagInfo[size]) {

        }

        TagInfoArray(query_size_t index, uint8_t state) :
                size(1), arrayPtr(new TagInfo[1]{TagInfo(index, state)}) {

        }

        TagInfoArray(const TagInfoArray &copyArray) : size(copyArray.size), arrayPtr(new TagInfo[copyArray.size]) {
            for (query_size_t i = 0; i < size; i++) {
                arrayPtr[i] = copyArray[i];
            }
        }

        TagInfoArray(TagInfoArray &&copyArray) noexcept: size(copyArray.size), arrayPtr(copyArray.arrayPtr.release()) {

        }

        TagInfoArray(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray) {
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

        void extend(const TagInfoArray &infoArray) {
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

        TagInfo &operator[](query_size_t i) const {
            return arrayPtr[i];
        }

        inline query_size_t getSize() const { return size; }

    private:
        static query_size_t getNewArraySize(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray) {
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

    private:
        std::unique_ptr<TagInfo[]> arrayPtr;
        query_size_t size;
    };

    class Tag {
    public:
        const static Tag EmptyTag;
        const static Tag TagWithNewInfo;
        const static Tag HeldObjectTag;
    protected:
        Tag(query_size_t index, uint8_t state) :
                array(index, state), isStartTag(true),
                refCount(1), alreadyReferred(false) {

        }

        Tag(const Tag &copyTag) :
                array(copyTag.array), refCount(1),
                isStartTag(copyTag.isStartTag), alreadyReferred(copyTag.alreadyReferred) {

        }

        Tag(const Tag &referree, const Tag &referrer) :
                array(referree.array, referrer.array), refCount(1),
                isStartTag(referree.isStartTag), alreadyReferred(referree.alreadyReferred) {

        }

        explicit Tag(TagInfoArray &&array) :
                array(std::move(array)), refCount(1),
                isStartTag(false), alreadyReferred(false) {

        }

        Tag() : array(), refCount(1), isStartTag(false), alreadyReferred(false) {

        }

    public:
        static Tag *create(query_size_t index, uint8_t state) {
            ++tagBalance;
            return new Tag(index, state);
        }

        static Tag *create(const Tag &referree, const Tag &referrer) {
            ++tagBalance;
            return new Tag(referree, referrer);
        }

        static Tag *copyWithoutStartMarks(const Tag &tag) {
            TagInfoArray result(tag.array.getSize());
            for (query_size_t i = 0; i < result.getSize(); i++) {
                const TagInfo &info = tag.array[i];
                result[i] = TagInfo(
                        info.index,
                        updateState(createState(false, false, false, true), info.state)
                );
            }

            ++tagBalance;
            return new Tag(std::move(result));
        }

        virtual Tag *share() {
            if (isStartTag) {
                return copyWithoutStartMarks(*this);
            }
            ref();
            return this;
        }

        virtual query_size_t getId() const {
            return 0;
        }

        inline void ref() {
            ++refCount;
        }

        inline void unref() {
            if (this == &EmptyTag || this == &HeldObjectTag || this == &TagWithNewInfo) {
                return;
            }

            if (--refCount == 0) {
                delete this;
            }
        }

        void visitFromUntaggedReferrer() const {
            for (query_size_t i = 0; i < array.getSize(); i++) {
                array[i].state = asVisitedFromUntagged(array[i].state);
            }
        }

        ~Tag() {
            --tagBalance;
        }

    public:
        bool isStartTag;
        bool alreadyReferred;
        TagInfoArray array;
        uint32_t refCount;
    };

    class ClassTag : public Tag {
    private:
        explicit ClassTag(query_size_t id) : Tag(), id(id) {

        }

        explicit ClassTag(query_size_t index, uint8_t state, query_size_t id) :
                Tag(index, state), id(id) {

        }

    public:
        query_size_t getId() const override {
            return id;
        }

        static Tag *create(query_size_t index) {
            ++tagBalance;
            return new ClassTag(index);
        }

        static Tag *create(query_size_t index, uint8_t state, query_size_t id) {
            ++tagBalance;
            return new ClassTag(index, state, id);
        }

    private:
        query_size_t id;
    };

    Tag *tagToPointer(jlong tag) {
        return reinterpret_cast<Tag *>(tag);
    }


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

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

    bool shouldMerge(const Tag &referree, const Tag &referrer) {
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

    bool handleReferrersWithNoInfo(const jlong *referrerTagPtr, jlong *tagPtr, bool setTagsWithNewInfo=false) {
        if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr)) {
            if (*tagPtr == 0) {
                *tagPtr = pointerToTag(&Tag::EmptyTag);
            } else if (!isEmptyTag(*tagPtr)) {
                Tag *referree = tagToPointer(*tagPtr);
                if (setTagsWithNewInfo && referree->alreadyReferred) {
                    referree->unref();
                    *tagPtr = pointerToTag(&Tag::TagWithNewInfo);
                } else {
                    referree->visitFromUntaggedReferrer();
                }
            }

            return true;
        } else if (isTagWithNewInfo(*referrerTagPtr) || *referrerTagPtr == 0 ||
                   tagToPointer(*referrerTagPtr)->getId() > 0) {
            return true;
        }

        return false;
    }

    bool tagsAreValidForMerge(jlong referree, jlong referrer) {
        return referree != referrer && shouldMerge(referree, referrer) &&
               tagToPointer(referree)->getId() == 0;
    }

    const Tag Tag::EmptyTag;
    const Tag Tag::TagWithNewInfo;
    const Tag Tag::HeldObjectTag;
    std::unordered_set<jlong> tagsWithNewInfo;

    jint JNICALL getTagsWithNewInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                       jlong referrerClassTag, jlong size, jlong *tagPtr,
                       const jlong *referrerTagPtr, jint length, void *_) {
        if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL ||
            isTagWithNewInfo(*tagPtr) || handleReferrersWithNoInfo(referrerTagPtr, tagPtr, true)) {
            return JVMTI_VISIT_OBJECTS;
        }

        Tag *referrer = tagToPointer(*referrerTagPtr);
        Tag *referree = tagToPointer(*tagPtr);
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(referrer->share());
        } else if (tagsAreValidForMerge(*tagPtr, *referrerTagPtr)) {
            if (referree->alreadyReferred) {
                referree->unref();
                *tagPtr = pointerToTag(&Tag::TagWithNewInfo);
            } else {
                *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
            }
        }

        referrer->alreadyReferred = true;
        return JVMTI_VISIT_OBJECTS;
    }

    jint JNICALL visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                const jlong *referrerTagPtr, jint length, void *_) {
        if (refKind == JVMTI_HEAP_REFERENCE_JNI_LOCAL || refKind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL ||
            handleReferrersWithNoInfo(referrerTagPtr, tagPtr)) {
            return JVMTI_VISIT_OBJECTS;
        } else if (*tagPtr == 0) {
            *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());
        } else if (isTagWithNewInfo(*tagPtr)) {
            *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());
            tagsWithNewInfo.insert(*tagPtr);
        } else if (tagsAreValidForMerge(*tagPtr, *referrerTagPtr)) {
            tagsWithNewInfo.erase(*tagPtr);
            *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
            tagsWithNewInfo.insert(*tagPtr);
        }

        return JVMTI_VISIT_OBJECTS;
    }

    jint JNICALL spreadInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr, const jlong *referrerTagPtr,
                            jint length, void *user_data) {
        if (refKind != JVMTI_HEAP_REFERENCE_JNI_LOCAL && refKind != JVMTI_HEAP_REFERENCE_JNI_GLOBAL &&
            *tagPtr != 0 && *referrerTagPtr != 0) {
            auto it = tagsWithNewInfo.find(*tagPtr);
            if (it != tagsWithNewInfo.end()) {
                tagsWithNewInfo.erase(it);
            }

            if (*referrerTagPtr != *tagPtr && shouldMerge(*tagPtr, *referrerTagPtr)) {
                *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
            }
        }

        return JVMTI_VISIT_OBJECTS;
    }

#pragma clang diagnostic pop

    jint JNICALL retagStartObjects(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
        Tag *pClassTag = tagToPointer(classTag);
        if (classTag != 0 && pClassTag->getId() > 0 && isTagWithNewInfo(*tagPtr)) {
            *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
            tagsWithNewInfo.insert(*tagPtr);
        }

        return JVMTI_ITERATION_CONTINUE;
    }

    jvmtiError findObjectsWithNewInfo(jvmtiEnv *jvmti) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&getTagsWithNewInfo);
        debug("find objects with new info");
        return jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    }

    jvmtiError collectObjectsWithNewInfo(jvmtiEnv *jvmti, std::vector<std::pair<jobject, jlong>> &objectsAndTags) {
        debug("collect objects with new info");
        return getObjectsByTags(jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)}, objectsAndTags);
    }

    jvmtiError retagStartObjectsOfClasses(jvmtiEnv *jvmti) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&retagStartObjects);
        debug("retag start objects");
        return jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, nullptr);
    }

    jvmtiError updateTagsWithNewInfo(jvmtiEnv *jvmti) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
        debug("tag heap");
        return jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    }

    jvmtiError createTagForObject(jvmtiEnv *jvmti, jobject object, size_t index) {
        jlong oldTag;
        jvmtiError err = jvmti->GetTag(object, &oldTag);
        if (err != JVMTI_ERROR_NONE) return err;
        Tag *tag = Tag::create(index, createState(true, true, false, false));
        if (oldTag != 0 && !isTagWithNewInfo(oldTag)) {
            tagToPointer(oldTag)->array.extend(tag->array);
            delete tag;
        } else {
            err = jvmti->SetTag(object, pointerToTag(tag));
        }

        return err;
    }

    jvmtiError retagStartObjects(jvmtiEnv *jvmti, const std::vector<jobject> &objects) {
        std::vector<std::pair<jobject, size_t>> objectsWithNewInfo;
        for (size_t i = 0; i < objects.size(); i++) {
            jlong oldTag;
            jvmtiError err = jvmti->GetTag(objects[i], &oldTag);
            if (err != JVMTI_ERROR_NONE) return err;

            if (isTagWithNewInfo(oldTag)) {
                objectsWithNewInfo.emplace_back(objects[i], i);
            }
        }

        for (auto objectToIndex : objectsWithNewInfo) {
            jvmtiError err = createTagForObject(jvmti, objectToIndex.first, objectToIndex.second);
            if (err != JVMTI_ERROR_NONE) return err;
        }

        return JVMTI_ERROR_NONE;
    }

    jvmtiError walkHeapFromObjects(jvmtiEnv *jvmti, const std::vector<std::pair<jobject, jlong>> &objectsAndTags) {
        jvmtiError err = JVMTI_ERROR_NONE;
        if (!objectsAndTags.empty()) {
            jvmtiHeapCallbacks cb;
            std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
            cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&spreadInfo);
            int heapWalksCnt = 0;
            for (auto &objectAndTag : objectsAndTags) {
                jlong tag;
                err = jvmti->GetTag(objectAndTag.first, &tag);
                if (err != JVMTI_ERROR_NONE) return err;

                if (tagsWithNewInfo.find(tag) != tagsWithNewInfo.end()) {
                    tagsWithNewInfo.erase(tag);
                    err = jvmti->FollowReferences(0, nullptr, objectAndTag.first, &cb, nullptr);
                    if (err != JVMTI_ERROR_NONE) return err;
                    debug(std::to_string(tagsWithNewInfo.size()).c_str());
                    heapWalksCnt++;
                }
            }
            debug(std::string("Total heap walks: " + std::to_string(heapWalksCnt)).c_str());
        }

        return err;
    }

    jvmtiError tagHeap(jvmtiEnv *jvmti) {
        jvmtiError err = findObjectsWithNewInfo(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        std::vector<std::pair<jobject, jlong>> objectsAndTags;
        err = collectObjectsWithNewInfo(jvmti, objectsAndTags);
        if (err != JVMTI_ERROR_NONE) return err;

        err = retagStartObjectsOfClasses(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        err = updateTagsWithNewInfo(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        return walkHeapFromObjects(jvmti, objectsAndTags);
    }

    jvmtiError tagHeap(jvmtiEnv *jvmti, const std::vector<jobject> &objects) {
        jvmtiError err = findObjectsWithNewInfo(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        std::vector<std::pair<jobject, jlong>> objectsAndTags;
        err = collectObjectsWithNewInfo(jvmti, objectsAndTags);
        if (err != JVMTI_ERROR_NONE) return err;

        err = retagStartObjects(jvmti, objects);
        if (err != JVMTI_ERROR_NONE) return err;

        err = updateTagsWithNewInfo(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        return walkHeapFromObjects(jvmti, objectsAndTags);
    }

    jint JNICALL visitObject(jlong classTag, jlong size, const jlong *tagPtr, jint length, void *userData) {
        if (*tagPtr == 0) {
            return JVMTI_ITERATION_CONTINUE;
        }

        Tag *tag = tagToPointer(*tagPtr);
        for (query_size_t i = 0; i < tag->array.getSize(); i++) {
            const TagInfo &info = tag->array[i];
            if (isRetained(info.state)) {
                reinterpret_cast<jlong *>(userData)[info.index] += size;
            }
        }

        return JVMTI_ITERATION_CONTINUE;
    }

    jint JNICALL countSizeAndRetagHeldObjects(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
        if (*tagPtr == 0) {
            return JVMTI_ITERATION_CONTINUE;
        }

        Tag *tag = tagToPointer(*tagPtr);
        *tagPtr = 0;
        for (query_size_t i = 0; i < tag->array.getSize(); i++) {
            const TagInfo &info = tag->array[i];
            if (isRetained(info.state)) {
                *tagPtr = pointerToTag(&Tag::HeldObjectTag);
                *reinterpret_cast<jlong *>(userData) += size;
            }
        }

        tag->unref();
        return JVMTI_ITERATION_CONTINUE;
    }

    jint JNICALL clearTag(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
        if (*tagPtr == 0) {
            return JVMTI_ITERATION_CONTINUE;
        }

        tagToPointer(*tagPtr)->unref();
        *tagPtr = 0;

        return JVMTI_ITERATION_CONTINUE;
    }

    jint JNICALL visitObjectForShallowAndRetainedSize(jlong classTag, jlong size, const jlong *tagPtr, jint length, void *userData) {
        if (*tagPtr == 0) {
            return JVMTI_ITERATION_CONTINUE;
        }

        Tag *tag = tagToPointer(*tagPtr);
        for (query_size_t i = 0; i < tag->array.getSize(); i++) {
            const TagInfo &info = tag->array[i];
            auto *arrays = reinterpret_cast<std::pair<jlong *, jlong *> *>(userData);
            if (isRetained(info.state)) {
                arrays->second[info.index] += size;
            }

            if (isStartObject(info.state)) {
                arrays->first[info.index] += size;
            }
        }

        return JVMTI_ITERATION_CONTINUE;
    }

    jint JNICALL tagObjectOfTaggedClass(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
        Tag *pClassTag = tagToPointer(classTag);
        if (classTag != 0 && pClassTag->getId() > 0) {
            if (*tagPtr == 0) {
                *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
            } else {
                Tag *oldTag = tagToPointer(*tagPtr);
                *tagPtr = pointerToTag(ClassTag::create(pClassTag->getId() - 1, createState(true, true, false, false),
                                                        oldTag->getId()));
                delete oldTag;
            }
        }

        return JVMTI_ITERATION_CONTINUE;
    }

    jvmtiError createTagsForObjects(jvmtiEnv *jvmti, const std::vector<jobject> &objects) {
        for (size_t i = 0; i < objects.size(); i++) {
            jvmtiError err = createTagForObject(jvmti, objects[i], i);
            if (err != JVMTI_ERROR_NONE) return err;
        }

        return JVMTI_ERROR_NONE;
    }

    jvmtiError createTagsForClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
        for (jsize i = 0; i < env->GetArrayLength(classesArray); i++) {
            jobject classObject = env->GetObjectArrayElement(classesArray, i);
            Tag *tag = ClassTag::create(static_cast<query_size_t>(i + 1));
            jvmtiError err = jvmti->SetTag(classObject, pointerToTag(tag));
            if (err != JVMTI_ERROR_NONE) return err;
        }

        return JVMTI_ERROR_NONE;
    }

    jvmtiError clearTags(jvmtiEnv *jvmti) {
        debug("clear tags");
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&clearTag);

        return jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);
    }

    jvmtiError calculateRetainedSizes(jvmtiEnv *jvmti, const std::vector<jobject> &objects, std::vector<jlong> &result) {
        std::set<jobject> unique(objects.begin(), objects.end());
        size_t count = objects.size();
        if (count != unique.size()) {
            fatal("Invalid argument: objects should be unique");
        }

        result.resize(static_cast<unsigned long>(count));
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObject);
        debug("calculate retained sizes");
        return jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, result.data());
    }

    jvmtiError estimateObjectsSizes(jvmtiEnv *jvmti, const std::vector<jobject> &objects, std::vector<jlong> &result) {
        jvmtiError err;
        err = createTagsForObjects(jvmti, objects);
        if (err != JVMTI_ERROR_NONE) return err;

        err = tagHeap(jvmti, objects);
        if (err != JVMTI_ERROR_NONE) return err;

        err = calculateRetainedSizes(jvmti, objects, result);
        if (err != JVMTI_ERROR_NONE) return err;

        err = clearTags(jvmti);
        if (tagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }

    jvmtiError calculateRetainedSize(jvmtiEnv *jvmti, jlong &retainedSize) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&countSizeAndRetagHeldObjects);
        debug("calculate retained size");
        retainedSize = 0;
        jvmtiError err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, &retainedSize);

        if (tagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }

    jvmtiError collectHeldObjects(jvmtiEnv *jvmti, std::vector<jobject> &heldObjects) {
        debug("collect held objects");
        return getObjectsByTags(jvmti, std::vector<jlong>{pointerToTag(&Tag::HeldObjectTag)}, heldObjects);
    }

    jvmtiError estimateObjectSize(jvmtiEnv *jvmti, jobject &object, jlong &retainedSize, std::vector<jobject> &heldObjects) {
        std::vector<jobject> objectVec{object};
        jvmtiError err = createTagsForObjects(jvmti, objectVec);
        if (err != JVMTI_ERROR_NONE) return err;

        err = tagHeap(jvmti, objectVec);
        if (err != JVMTI_ERROR_NONE) return err;

        err = calculateRetainedSize(jvmti, retainedSize);
        if (err != JVMTI_ERROR_NONE) return err;

        err = collectHeldObjects(jvmti, heldObjects);
        if (err != JVMTI_ERROR_NONE) return err;

        return removeAllTagsFromHeap(jvmti, nullptr);
    }

    jobjectArray createResultObject(JNIEnv *env, jlong retainedSize, const std::vector<jobject> &heldObjects) {
        jint objectsCount = static_cast<jint>(heldObjects.size());
        jclass langObject = env->FindClass("java/lang/Object");
        jobjectArray resultObjects = env->NewObjectArray(objectsCount, langObject, nullptr);

        for (jsize i = 0; i < objectsCount; ++i) {
            env->SetObjectArrayElement(resultObjects, i, heldObjects[i]);
        }

        jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
        env->SetObjectArrayElement(result, 0, toJavaArray(env, retainedSize));
        env->SetObjectArrayElement(result, 1, resultObjects);

        return result;
    }

    jvmtiError tagObjectsOfClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&tagObjectOfTaggedClass);

        debug("tag objects of classes");
        jvmtiError err = createTagsForClasses(env, jvmti, classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        return jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);
    }

    jvmtiError getRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray, std::vector<jlong> &result) {
        jvmtiError err = tagObjectsOfClasses(env, jvmti, classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        err = tagHeap(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        result.resize(env->GetArrayLength(classesArray));
        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObject);
        debug("calculate retained sizes");
        err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, result.data());
        if (err != JVMTI_ERROR_NONE) return err;

        err = clearTags(jvmti);
        if (tagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }

    jvmtiError getShallowAndRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray,
                                                         std::vector<jlong> &shallowSizes,
                                                         std::vector<jlong> &retainedSizes) {
        jvmtiError err = tagObjectsOfClasses(env, jvmti, classesArray);
        if (err != JVMTI_ERROR_NONE) return err;

        err = tagHeap(jvmti);
        if (err != JVMTI_ERROR_NONE) return err;

        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObjectForShallowAndRetainedSize);
        retainedSizes.resize(env->GetArrayLength(classesArray));
        shallowSizes.resize(env->GetArrayLength(classesArray));
        std::pair<jlong *, jlong *> arrays = std::make_pair(shallowSizes.data(), retainedSizes.data());
        debug("calculate shallow and retained sizes");
        err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, &arrays);
        if (err != JVMTI_ERROR_NONE) return err;

        err = clearTags(jvmti);
        if (tagBalance != 0) {
            fatal("MEMORY LEAK FOUND!");
        }

        return err;
    }
}

jobjectArray estimateObjectSize(JNIEnv *env, jvmtiEnv *jvmti, jobject object) {
    std::vector<jobject> objects;
    objects.push_back(object);
    std::vector<jobject> heldObjects;
    jlong retainedSize;
    jvmtiError err = estimateObjectSize(jvmti, object, retainedSize, heldObjects);

    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate object size");
    }

    return createResultObject(env, retainedSize, heldObjects);
}

jlongArray estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray objects) {
    debug("start estimate objects sizes");
    debug("convert java array to vector");
    std::vector<jobject> javaObjects;
    fromJavaArray(env, objects, javaObjects);
    std::vector<jlong> result;
    jvmtiError err = estimateObjectsSizes(jvmti, javaObjects, result);
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate objects size");
        return env->NewLongArray(0);
    }

    return toJavaArray(env, result);
}

jlongArray getRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
    std::vector<jlong> result;
    jvmtiError err = getRetainedSizeByClasses(env, jvmti, classesArray, result);
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
        return env->NewLongArray(0);
    }

    return toJavaArray(env, result);
}

jobjectArray getShallowAndRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
    std::vector<jlong> shallowSizes;
    std::vector<jlong> retainedSizes;
    jvmtiError err = getShallowAndRetainedSizeByClasses(env, jvmti, classesArray,
            shallowSizes, retainedSizes);

    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
        return result;
    }

    env->SetObjectArrayElement(result, 0, toJavaArray(env, shallowSizes));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, retainedSizes));
    return result;
}
