// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include "objects_size.h"
#include "utils.h"
#include "log.h"
#include "size_by_classes.h"

using query_size_t = uint16_t;

static jlong tagBalance = 0;
static jlong visitedObjectsBalance = 0;

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

protected:
    Tag(query_size_t index, uint8_t state) :
        array(index, state), pendingChangesCnt(0), isStartTag(true), refCount(1) {

    }

    Tag(const Tag &copyTag) :
        array(copyTag.array), refCount(1), pendingChangesCnt(0), isStartTag(copyTag.isStartTag) {

    }

    Tag(const Tag &referree, const Tag &referrer) :
        array(referree.array, referrer.array), refCount(1), pendingChangesCnt(0), isStartTag(referree.isStartTag) {

    }

    explicit Tag(TagInfoArray &&array) :
            array(std::move(array)), refCount(1), pendingChangesCnt(0), isStartTag(false) {

    }

    Tag() : array(), refCount(1), pendingChangesCnt(0), isStartTag(false) {

    }

public:
    static Tag *create(query_size_t index, uint8_t state) {
        ++tagBalance;
        return new Tag(index, state);
    }

    static Tag *create(const Tag &copyTag) {
        ++tagBalance;
        return new Tag(copyTag);
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
    TagInfoArray array;
    uint8_t pendingChangesCnt;
    uint32_t refCount;
};

const Tag Tag::EmptyTag;

class ClassTag : public Tag {
private:
    explicit ClassTag(query_size_t index) : Tag(), id(index + 1) {

    }

public:
    query_size_t getId() const override {
        return id;
    }

    static Tag *create(query_size_t index) {
        ++tagBalance;
        return new ClassTag(index);
    }

private:
    query_size_t id;
};

static Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"


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

static bool isEmptyTag(jlong tag) {
    return tag == pointerToTag(&Tag::EmptyTag);
}

static bool shouldMerge(jlong referree, jlong referrer) {
    return shouldMerge(*tagToPointer(referree), *tagToPointer(referrer));
}

static void safeUnref(jlong tag) {
    if (!isEmptyTag(tag)) {
        tagToPointer(tag)->unref();
    }
}

static Tag *merge(jlong referreeTag, jlong referrerTag) {
    Tag *result = Tag::create(*tagToPointer(referreeTag), *tagToPointer(referrerTag));
    safeUnref(referreeTag);

    return result;
}

jint JNICALL visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            const jlong *referrerTagPtr, jint length, void *_) {
    visitedObjectsBalance++;
    if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr)) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(&Tag::EmptyTag);
        } else {
            tagToPointer(*tagPtr)->visitFromUntaggedReferrer();
        }

        return JVMTI_VISIT_OBJECTS;
    }

    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(tagToPointer(*referrerTagPtr)->share());;
    } else if (*referrerTagPtr != *tagPtr) {
        *tagPtr = pointerToTag(merge(*tagPtr, *referrerTagPtr));
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL updatePendingChanges(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                  jlong referrerClassTag, jlong size, jlong *tagPtr,
                                  const jlong *referrerTagPtr, jint length, void *_) {
    if (referrerTagPtr == nullptr || isEmptyTag(*referrerTagPtr) ||
        *referrerTagPtr == 0 || *tagPtr == 0) {
        return JVMTI_VISIT_OBJECTS;
    } else if (*referrerTagPtr != *tagPtr) {
        Tag *referreeTag = tagToPointer(*tagPtr);
        if (shouldMerge(*tagPtr, *referrerTagPtr)) {
            if (referreeTag->pendingChangesCnt == 0) {
                Tag *tag = Tag::create(*referreeTag);
                safeUnref(*tagPtr);
                *tagPtr = pointerToTag(tag);
                referreeTag = tag;
            }

            referreeTag->pendingChangesCnt++;
        }
    }

    return JVMTI_VISIT_OBJECTS;
}

jint JNICALL applyChanges(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                          jlong referrerClassTag, jlong size, jlong *tagPtr,
                          const jlong *referrerTagPtr, jint length, void *_) {
    visitedObjectsBalance--;
    if (*tagPtr == 0 || isEmptyTag(*tagPtr)) {
        return JVMTI_VISIT_OBJECTS;
    }

    Tag *referreeTag = tagToPointer(*tagPtr);
    uint8_t changes = referreeTag->pendingChangesCnt;
    if (referrerTagPtr != nullptr && *referrerTagPtr != 0 && !isEmptyTag(*referrerTagPtr) &&
        *referrerTagPtr != *tagPtr && shouldMerge(*tagPtr, *referrerTagPtr)) {
        Tag *tag = merge(*tagPtr, *referrerTagPtr);

        if (changes > 0) {
            tag->pendingChangesCnt = --changes;
        }

        *tagPtr = pointerToTag(tag);
    }

    if (changes == 0) {
        return JVMTI_VISIT_OBJECTS;
    }
    return 0;
}

#pragma clang diagnostic pop

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
        *tagPtr = pointerToTag(Tag::create(pClassTag->getId() - 1, createState(true, true, false, false)));
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
        Tag *tag = ClassTag::create(static_cast<query_size_t>(i));
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

jvmtiError tagHeap(jvmtiEnv *jvmti) {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    debug("tag heap");
    jvmtiError err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&updatePendingChanges);
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&applyChanges);
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);

//    if (visitedObjectsBalance != 0) {
//        fatal("NOT ALL OBJECTS WERE VISITED");
//    }

    return err;
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
