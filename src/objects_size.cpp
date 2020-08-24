// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include "objects_size.h"
#include "utils.h"
#include "log.h"
#include "size_by_classes.h"

using query_size_t = uint16_t;

static jlong tagBalance = 0;

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

uint8_t createState(bool isStartObject, bool isInSubtree, bool isReachableOutside, bool isAlreadyVisited=true) {
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
            isReachableOutside(currentState) || (!isStartObject(referrerState) && isReachableOutside(referrerState)),
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
    TagInfoArray() : array(nullptr), size(0) {

    }

    explicit TagInfoArray(query_size_t size) : size(size) {
        array = new TagInfo[size];
    }

    TagInfoArray(query_size_t index, uint8_t state) : size(1) {
        array = new TagInfo[1] { TagInfo(index, state) };
    }

    TagInfoArray(const TagInfoArray &copyArray) {
        size = copyArray.size;
        array = new TagInfo[size];
        for (query_size_t i = 0; i < size; i++) {
            array[i] = copyArray[i];
        }
    }

    TagInfoArray(TagInfoArray &&copyArray) noexcept {
        size = copyArray.size;
        array = copyArray.array;
        copyArray.size = 0;
        copyArray.array = nullptr;
    }

    TagInfoArray(const TagInfoArray &referreeArray, const TagInfoArray &referrerArray) {
        size = getNewArraySize(referreeArray, referrerArray);
        array = new TagInfo[size];
        bool alreadyVisited = referreeArray.size == 0 || isAlreadyVisited(referreeArray.array[0].state);
        query_size_t i = 0;
        query_size_t j = 0;
        query_size_t k = 0;
        while (i < referreeArray.size && j < referrerArray.size) {
            if (referreeArray[i].index == referrerArray[j].index) {
                array[k++] = TagInfo(
                            referreeArray[i].index,
                            updateState(referreeArray[i].state, referrerArray[j].state)
                        );
                i++;
                j++;
            } else if (referreeArray[i].index < referrerArray[j].index) {
                array[k++] = TagInfo(
                            referreeArray[i].index,
                            updateState(referreeArray[i].state,createState(false, false, true))
                        );
                i++;
            } else {
                array[k++] = TagInfo(
                            referrerArray[j].index,
                            updateState(createState(false, false,  alreadyVisited, true), referrerArray[j].state)
                        );
                j++;
            }
        }

        while (j < referrerArray.size) {
            array[k++] = TagInfo(
                        referrerArray[j].index,
                        updateState(createState(false, false,  alreadyVisited), referrerArray[j].state)
                    );
            j++;
        }

        while (i < referreeArray.size) {
            array[k++] = TagInfo(
                        referreeArray[i].index,
                        updateState(referreeArray[i].state, createState(false, false, true))
                    );
            i++;
        }
    }

    void extend(const TagInfoArray &infoArray) {
        auto *newArray = new TagInfo[size + infoArray.size];
        for (query_size_t i = 0; i < size; i++) {
            newArray[i] = array[i];
        }

        for (query_size_t i = 0; i < infoArray.size; i++) {
            newArray[size + i] = infoArray[i];
        }

        size += infoArray.size;
        delete array;
        array = newArray;
    }

    ~TagInfoArray() {
        delete []array;
    }

    TagInfo &operator[](query_size_t i) const {
        return array[i];
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
    TagInfo *array;
    query_size_t size;
};

class Tag {
public:
    const static Tag EmptyTag;
    const static Tag TagWithNewInfo;

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
                    updateState(createState(false, false,  false, true), info.state)
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

static Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static bool isEmptyTag(jlong tag) {
    return tag == pointerToTag(&Tag::EmptyTag);
}

static bool isTagWithNewInfo(jlong tag) {
    return tag == pointerToTag(&Tag::TagWithNewInfo);
}

static Tag *merge(jlong referreeTag, jlong referrerTag) {
    Tag *result = Tag::create(*tagToPointer(referreeTag), *tagToPointer(referrerTag));
    if (!isEmptyTag(referreeTag)) {
        tagToPointer(referreeTag)->unref();
    }

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
            i++;
            j++;
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

static bool shouldMerge(jlong referree, jlong referrer) {
    return shouldMerge(*tagToPointer(referree), *tagToPointer(referrer));
}

const Tag Tag::EmptyTag;
const Tag Tag::TagWithNewInfo;
static std::unordered_set<jlong> tagsWithNewInfo;

jint JNICALL getTagsWithNewInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                jlong referrerClassTag, jlong size, jlong *tagPtr,
                                const jlong *referrerTagPtr, jint length, void *_) {
    if (isTagWithNewInfo(*tagPtr)) {
        return JVMTI_VISIT_OBJECTS;
    }

    if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr)) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(&Tag::EmptyTag);
        } else {
            tagToPointer(*tagPtr)->visitFromUntaggedReferrer();
        }

        return JVMTI_VISIT_OBJECTS;
    }

    Tag *referrer = tagToPointer(*referrerTagPtr);
    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(referrer->share());
    } else if (*referrerTagPtr != *tagPtr) {
        Tag *referree = tagToPointer(*tagPtr);
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
    if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr) || *referrerTagPtr == 0) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(&Tag::EmptyTag);
        } else {
            tagToPointer(*tagPtr)->visitFromUntaggedReferrer();
        }

        return JVMTI_VISIT_OBJECTS;
    }

    if (*tagPtr == 0 || isTagWithNewInfo(*tagPtr)) {
        *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());
    } else if (*referrerTagPtr != *tagPtr && shouldMerge(*tagPtr, *referrerTagPtr)) {
        jlong newTag = pointerToTag(merge(*tagPtr, *referrerTagPtr));
        tagsWithNewInfo.erase(*tagPtr);
        tagsWithNewInfo.insert(newTag);
        *tagPtr = newTag;
    }

    return JVMTI_VISIT_OBJECTS;
}

static bool newHeapWalk = false;
jint JNICALL spreadInfo(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                        jlong referrerClassTag, jlong size, jlong *tagPtr, const jlong *referrerTagPtr,
                        jint length, void *user_data) {
    if (newHeapWalk) {
        newHeapWalk = false;
        bool *iterationStopped = reinterpret_cast<bool *>(user_data);
        if (tagsWithNewInfo.find(*tagPtr) == tagsWithNewInfo.end()) {
            *iterationStopped = true;
            return JVMTI_VISIT_ABORT;
        } else {
            *iterationStopped = false;
        }
    }

    if (*tagPtr != 0 && *referrerTagPtr != 0) {
        auto it = tagsWithNewInfo.find(*tagPtr);
        if (it != tagsWithNewInfo.end()) {
            tagsWithNewInfo.erase(it);
        }

        if (*referrerTagPtr != *tagPtr) {
            *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
        }
    }

    return JVMTI_VISIT_OBJECTS;
}

#pragma clang diagnostic pop

jint JNICALL retagStartObjects(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    Tag *pClassTag = tagToPointer(classTag);
    if (classTag != 0 && pClassTag->getId() > 0 && isTagWithNewInfo(*tagPtr))  {
        printf("RETAG\n");
        *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
    }

    return JVMTI_ITERATION_CONTINUE;
}

static jvmtiError tagHeap(jvmtiEnv *jvmti) {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&getTagsWithNewInfo);
    debug("tag objects with new info");
    jvmtiError err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    std::vector<std::pair<jobject, jlong>> objectsAndTags;
    debug("collect objects with new info");
    err = getObjectsByTags(jvmti, std::vector<jlong>{pointerToTag(&Tag::TagWithNewInfo)},objectsAndTags);
    if (err != JVMTI_ERROR_NONE) return err;

    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&retagStartObjects);
    debug("retag start objects");
    err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    printf("Size1: %lu\n", tagsWithNewInfo.size());
    printf("Size2: %lu\n", objectsAndTags.size());
    if (!objectsAndTags.empty()) {
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&spreadInfo);
        int heapWalksCnt = 0;
        for (auto &objectAndTag : objectsAndTags) {
            newHeapWalk = true;
            bool iterationStopped = false;
            err = jvmti->FollowReferences(0, nullptr, objectAndTag.first, &cb, &iterationStopped);
            if (err != JVMTI_ERROR_NONE) return err;

            if (!iterationStopped) {
                debug(std::to_string(tagsWithNewInfo.size()).data());
                heapWalksCnt++;
            }
        }
        debug(std::string("Total heap walks: " + std::to_string(heapWalksCnt)).data());
    }

    return err;
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

jint JNICALL clearTag(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    if (!isEmptyTag(*tagPtr)) {
        tagToPointer(*tagPtr)->unref();
    }
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
    if (classTag != 0 && pClassTag->getId() > 0)  {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
        } else {
            Tag *oldTag = tagToPointer(*tagPtr);
            *tagPtr = pointerToTag(ClassTag::create(pClassTag->getId() - 1, createState(true, true, false, false), oldTag->getId()));
            delete oldTag;
        }
    }

    return JVMTI_ITERATION_CONTINUE;
}

static jvmtiError createTagsForObjects(jvmtiEnv *jvmti, const std::vector<jobject> &objects) {
    for (size_t i = 0; i < objects.size(); i++) {
        jlong oldTag;
        jvmtiError err = jvmti->GetTag(objects[i], &oldTag);
        if (err != JVMTI_ERROR_NONE) return err;
        Tag *tag = Tag::create(i, createState(true, true, false, false));
        if (oldTag != 0) {
            tagToPointer(oldTag)->array.extend(tag->array);
            delete tag;
        } else {
            err = jvmti->SetTag(objects[i], pointerToTag(tag));
        }
        if (err != JVMTI_ERROR_NONE) return err;
    }

    return JVMTI_ERROR_NONE;
}

static jvmtiError createTagsForClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
    for (jsize i = 0; i < env->GetArrayLength(classesArray); i++) {
        jobject classObject = env->GetObjectArrayElement(classesArray, i);
        Tag *tag = ClassTag::create(static_cast<query_size_t>(i + 1));
        jvmtiError err = jvmti->SetTag(classObject, pointerToTag(tag));
        if (err != JVMTI_ERROR_NONE) return err;
    }

    return JVMTI_ERROR_NONE;
}

static jvmtiError clearTags(jvmtiEnv *jvmti) {
    debug("clear tags");
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&clearTag);

    return jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);
}

static jvmtiError estimateObjectsSizes(jvmtiEnv *jvmti, std::vector<jobject> &objects, std::vector<jlong> &result) {
    jvmtiError err;
    std::set<jobject> unique(objects.begin(), objects.end());
    size_t count = objects.size();
    if (count != unique.size()) {
        fatal("Invalid argument: objects should be unique");
    }

    tagBalance = 0;
    err = createTagsForObjects(jvmti, objects);
    if (err != JVMTI_ERROR_NONE) return err;

    err = tagHeap(jvmti);
    if (err != JVMTI_ERROR_NONE) return err;
    result.resize(static_cast<unsigned long>(count));

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

jlong estimateObjectSize(jvmtiEnv *jvmti, jobject object) {
    std::vector<jobject> objects;
    objects.push_back(object);
    std::vector<jlong> result;
    jvmtiError err = estimateObjectsSizes(jvmti, objects, result);
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate object size");
        return -1;
    }

    if (result.size() != 1) {
        fatal("Unexpected result format: vector with one element expected.");
        return -1;
    }

    return result[0];
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

static jvmtiError tagObjectsOfClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&tagObjectOfTaggedClass);

    debug("tag objects of classes");
    jvmtiError err = createTagsForClasses(env, jvmti, classesArray);
    if (err != JVMTI_ERROR_NONE) return err;

    return jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);
}

static jvmtiError getRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray, std::vector<jlong> &result) {
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

static jvmtiError getShallowAndRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray,
                                                     std::vector<jlong> &shallowSizes, std::vector<jlong> &retainedSizes) {
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
