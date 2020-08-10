// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include "objects_size.h"
#include "utils.h"
#include "log.h"
#include "size_by_classes.h"

using result_index = uint16_t;

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

    TagInfo(result_index index, uint8_t state) :
        index(index), state(state) {

    }

    result_index index;
    uint8_t state;
};

class TagInfoArray {
public:
    TagInfoArray() : array(nullptr), size(0) {

    }

    TagInfoArray(result_index index, uint8_t state) {
        array = new TagInfo[1] { TagInfo(index, state) };
        size = 1;
    }

    TagInfoArray(const TagInfoArray &copyArray) {
        size = copyArray.size;
        array = new TagInfo[size];
        for (result_index i = 0; i < size; i++) {
            array[i] = TagInfo(
                        copyArray[i].index,
                        updateState(createState(false, false,  false, true), copyArray[i].state)
                    );
        }
    }

    TagInfoArray(const TagInfoArray &referreeArray,
                 const TagInfoArray &referrerArray) {
        size = getNewArraySize(referreeArray, referrerArray);
        array = new TagInfo[size];
        bool alreadyVisited = referreeArray.size == 0 || isAlreadyVisited(referreeArray.array[0].state);
        result_index i = 0;
        result_index j = 0;
        result_index k = 0;
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

        while  (j < referrerArray.size) {
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
        for (result_index i = 0; i < size; i++) {
            newArray[i] = array[i];
        }

        for (result_index i = 0; i < infoArray.size; i++) {
            newArray[size + i] = infoArray[i];
        }

        size += infoArray.size;
        delete array;
        array = newArray;
    }

    ~TagInfoArray() {
        delete []array;
    }

    TagInfo &operator[](result_index i) const {
        return array[i];
    }

    result_index getSize() { return size; }

private:
    static result_index getNewArraySize(const TagInfoArray &referreeArray,
                                        const TagInfoArray &referrerArray) {
        result_index i = 0;
        result_index j = 0;
        result_index size = 0;
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
    result_index size;
};

typedef struct Tag {
protected:
    Tag (result_index index, uint8_t state) : array(index, state), tagState(0) {
        initState(false, true);
    }

    Tag(const Tag &copyTag) : array(copyTag.array), tagState(0) {
        initState(false, false);
    }

    Tag (const Tag &referree, const Tag &referrer) :
        array(referree.array, referrer.array), tagState(0) {
        initState(false, referree.isStartTag());
    }

public:
    explicit Tag(bool isShared) : array(), tagState(0) {
        initState(isShared, false);
    }

    static Tag *create(result_index index, uint8_t state) {
        ++tagBalance;
        return new Tag(index, state);
    }

    static Tag *create(const Tag &copyTag) {
        ++tagBalance;
        return new Tag(copyTag);
    }

    static Tag *create(const Tag &referree,
                       const Tag &referrer) {
        ++tagBalance;
        return new Tag(referree, referrer);
    }

    virtual result_index getId() const {
        return 0;
    }

    void visitFromUntaggedReferrer() {
        for (result_index i = 0; i < array.getSize(); i++) {
            array[i].state = asVisitedFromUntagged(array[i].state);
        }
    }

    void setShared() {
        tagState |= 1u;
    }

    bool isShared() const {
        return (tagState & 1u) != 0u;
    }

    bool isStartTag() const {
        return (tagState & (1u << 1u)) != 0u;
    }

    ~Tag() {
        --tagBalance;
    }

private:
    void initState(bool isShared, bool isStartTag) {
        if (isShared) {
            tagState |= 1u;
        }

        if (isStartTag) {
            tagState |= 1u << 1u;
        }
    }

private:
    uint8_t tagState;

public:
    TagInfoArray array;
} Tag;

class ClassTag : public Tag {
private:
    explicit ClassTag(result_index index) : Tag(false), id(index + 1) {

    }

public:
    result_index getId() const override {
        return id;
    }

    static Tag *create(result_index index) {
        ++tagBalance;
        return new ClassTag(index);
    }

private:
    result_index id;
};

static Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

static Tag EmptyTag(true);

jint JNICALL visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                            jlong referrerClassTag, jlong size, jlong *tagPtr,
                            jlong *referrerTagPtr, jint length, void *_) { // NOLINT(readability-non-const-parameter)
    if (referrerTagPtr == nullptr || *referrerTagPtr == pointerToTag(&EmptyTag)) {
        if (*tagPtr == 0) {
            *tagPtr = pointerToTag(&EmptyTag);
        } else {
            tagToPointer(*tagPtr)->visitFromUntaggedReferrer();
        }

        return JVMTI_VISIT_OBJECTS;
    }

    if (*tagPtr == 0) {
        Tag *referrerTag = tagToPointer(*referrerTagPtr);
        if (*referrerTagPtr != 0 && referrerTag->isStartTag()) {
            *tagPtr = pointerToTag(Tag::create(*referrerTag));
        } else {
            referrerTag->setShared();
            *tagPtr = *referrerTagPtr;
        }
    } else if (*referrerTagPtr != *tagPtr) {
        Tag *referreeTag = tagToPointer(*tagPtr);
        Tag *referrerTag = tagToPointer(*referrerTagPtr);
        Tag *tag = Tag::create(*referreeTag, *referrerTag);
        if (!referreeTag->isShared()) {
            delete referreeTag;
        }
        *tagPtr = pointerToTag(tag);
    }

    return JVMTI_VISIT_OBJECTS;
}

#pragma clang diagnostic pop

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr,
                         jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    for (result_index i = 0; i < tag->array.getSize(); i++) {
        const TagInfo &info = tag->array[i];
        if (isRetained(info.state)) {
            reinterpret_cast<jlong *>(userData)[info.index] += size;
        }
    }

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL clearTag(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    auto *tagsSet = reinterpret_cast<std::unordered_set<jlong> *>(userData);
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    if (tag != &EmptyTag && tagsSet->insert(*tagPtr).second) {
        delete tag;
    }
    *tagPtr = 0;

    return JVMTI_ITERATION_CONTINUE;
}

jint JNICALL visitObjectForShallowAndRetainedSize(jlong classTag, jlong size, jlong *tagPtr,
                                                  jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    for (result_index i = 0; i < tag->array.getSize(); i++) {
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

jint JNICALL tagObjectOfTaggedClass(jlong classTag, jlong size, jlong *tagPtr,
                                    jint length, void *userData) {
    Tag *pClassTag = tagToPointer(classTag);
    if (classTag != 0 && pClassTag->getId() > 0)  {
        Tag *tag = Tag::create(pClassTag->getId() - 1, createState(true, true, false, false));
        *tagPtr = pointerToTag(tag);
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
        Tag *tag = ClassTag::create(static_cast<result_index>(i));
        jvmtiError err = jvmti->SetTag(classObject, pointerToTag(tag));
        if (err != JVMTI_ERROR_NONE) return err;
    }

    return JVMTI_ERROR_NONE;
}

static jvmtiError clearTags(jvmtiEnv *jvmti) {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&clearTag);

    std::unordered_set<jlong> tagsSet;
    return jvmti->IterateThroughHeap(0, nullptr, &cb, &tagsSet);
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

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObject);

    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;
    result.resize(static_cast<unsigned long>(count));
    debug("calculate retained sizes");
    err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, result.data());
    if (err != JVMTI_ERROR_NONE) return err;
    debug("clearing");
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

    err = jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);

    return err;
}

static jvmtiError getRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray, std::vector<jlong> &result) {
    jvmtiError err = tagObjectsOfClasses(env, jvmti, classesArray);
    if (err != JVMTI_ERROR_NONE) return err;

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObject);

    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;


    debug("calculate retained sizes");
    result.resize(env->GetArrayLength(classesArray));
    err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, result.data());
    if (err != JVMTI_ERROR_NONE) return err;
    debug("clear tags");
    err = clearTags(jvmti);

    return err;
}

static jvmtiError getShallowAndRetainedSizeByClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray,
                                                     std::vector<jlong> &shallowSizes, std::vector<jlong> &retainedSizes) {
    jvmtiError err = tagObjectsOfClasses(env, jvmti, classesArray);
    if (err != JVMTI_ERROR_NONE) return err;

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObjectForShallowAndRetainedSize);

    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;

    debug("calculate shallow and retained sizes");
    retainedSizes.resize(env->GetArrayLength(classesArray));
    shallowSizes.resize(env->GetArrayLength(classesArray));
    std::pair<jlong *, jlong *> arrays = std::make_pair(shallowSizes.data(), retainedSizes.data());
    err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, &arrays);
    if (err != JVMTI_ERROR_NONE) return err;
    err = clearTags(jvmti);

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
