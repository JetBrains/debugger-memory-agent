// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include "objects_size.h"
#include "utils.h"
#include "object_size.h"
#include "log.h"

static jlong tagBalance = 0;

typedef struct Tag {
private:
    explicit Tag(jlong s) : size(s) {}

public:

    jlong size;
    std::unordered_map<jint, uint8_t> states;

    static Tag *create(jlong size) {
        ++tagBalance;
        return new Tag(size);
    }

    ~Tag() {
        --tagBalance;
    }
} Tag;

static Tag *tagToPointer(jlong tag) {
    return reinterpret_cast<Tag *>(tag);
}

bool isStartObject(uint8_t state) {
    return (state & 1u) != 0u;
}

bool isInSubtree(uint8_t state) {
    return (state & (1u << 1u)) != 0u;
}

bool isReachableOutside(uint8_t state) {
    return (state & (1u << 2u)) != 0u;
}

uint8_t create_state(bool isStartObject, bool isInSubtree, bool isReachableOutside) {
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

    return state;
}

bool isRetained(uint8_t state) {
    return isStartObject(state) || (isInSubtree(state) && !isReachableOutside(state));
}

uint8_t defaultState() {
    return create_state(false, false, false);
}

uint8_t updateState(uint8_t currentState, uint8_t referrerState) {
    return create_state(
            isStartObject(currentState),
            isInSubtree(currentState) || isInSubtree(referrerState),
            isReachableOutside(currentState) || (!isStartObject(referrerState) && isReachableOutside(referrerState))
    );
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

jint visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                    jlong referrerClassTag, jlong size, jlong *tagPtr,
                    jlong *referrerTagPtr, jint length, void *_) { // NOLINT(readability-non-const-parameter)

    bool alreadyVisited = *tagPtr != 0;
    if (!alreadyVisited) {
        *tagPtr = pointerToTag(Tag::create(size));
    }


    if (referrerTagPtr != nullptr) {
        // not a gc root
        if (*referrerTagPtr == 0) {
            error("Unexpected state: referrer has no tag");
            return JVMTI_VISIT_ABORT;
        }

        Tag *referrerTag = tagToPointer(*referrerTagPtr);
        Tag *refereeTag = tagToPointer(*tagPtr);
        for (auto &entry: refereeTag->states) {
            if (referrerTag->states.find(entry.first) == referrerTag->states.end()) {
                entry.second = updateState(entry.second, create_state(false, false, true));
            }
        }

        for (const auto &entry: referrerTag->states) {
            auto beforeIter = refereeTag->states.find(entry.first);
            uint8_t currentState = beforeIter == refereeTag->states.end() ? create_state(false, false, alreadyVisited)
                                                                          : beforeIter->second;
            if (size == 152) {
                int a = 100;
            }
            refereeTag->states[entry.first] = updateState(currentState, entry.second);
        }
    }

    return JVMTI_VISIT_OBJECTS;
}

#pragma clang diagnostic pop

jint visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    Tag *tag = tagToPointer(*tagPtr);
    for (const auto &entry: tag->states) {
        if (isRetained(entry.second)) {
            reinterpret_cast<jlong *>(userData)[entry.first] += tag->size;
        }
    }

    delete tag;
    *tagPtr = 0;
    return JVMTI_ITERATION_CONTINUE;
}

jvmtiError estimateObjectsSizedForUnique(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jobject> &objects,
                                         std::vector<jlong> &result) {
    jvmtiError err;
    std::set<jobject> unique(objects.begin(), objects.end());
    size_t count = objects.size();
    if (count != unique.size()) {
        fatal("Invalid argument: objects should be unique");
    }

    tagBalance = 0;
    for (int i = 0; i < count; i++) {
        jobject object = objects[i];
        jlong size = -1;
        err = jvmti->GetObjectSize(object, &size);
        handleError(jvmti, err, "Could not determine object size");
        Tag *tag = Tag::create(size);
        tag->states[i] = create_state(true, true, false);
        err = jvmti->SetTag(objects[i], pointerToTag(tag));
        if (err != JVMTI_ERROR_NONE) return err;
    }

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    cb.heap_iteration_callback = visitObject;
    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;
    result.resize(static_cast<unsigned long>(count));
    debug("calculate retained sizes");
    err = jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, &cb, result.data());
    if (tagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }
    return err;
}


static void distinct(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jobject> &in, std::vector<jobject> &out) {

    auto equal = [env](const jobject o1, const jobject &o2) { return env->IsSameObject(o1, o2); };
    auto hash = [jvmti](const jobject obj) {
        jint result = 0;
        jvmtiError err = jvmti->GetObjectHashCode(obj, &result);
        if (err != JVMTI_ERROR_NONE) {
            error("Could not receive object hash code");
        }
        return result;
    };
    std::unordered_set<jobject, decltype(hash), decltype(equal)> unique(10, hash, equal);
    out.assign(unique.begin(), unique.end());
}

jvmtiError estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jobject> &objects,
                                std::vector<jlong> &result) {
    std::vector<jobject> unique;
    distinct(env, jvmti, objects, unique);
    std::unordered_map<jobject, jint> objectToIndexInResult;
    std::cout << unique.size();
    bool check = env->IsSameObject(objects[0], objects[1]);
    if (unique.size() < objects.size()) {
        debug("remove duplicates in array");
    }
    auto uniqueCount = unique.size();
    for (jint i = 0; i < uniqueCount; ++i) {
        objectToIndexInResult[unique[i]] = i;
    }
    std::vector<jlong> uniqueSizes;
    jvmtiError err = estimateObjectsSizedForUnique(env, jvmti, unique, uniqueSizes);
    if (err != JVMTI_ERROR_NONE) return err;
    result.resize(objects.size());
    debug("convert to array with duplicates");
    for (auto i = 0; i < objects.size(); ++i) {
        result[i] = uniqueSizes[objectToIndexInResult[objects[i]]];
    }

    return JVMTI_ERROR_NONE;
}

jlong estimateObjectSize(JNIEnv *env, jvmtiEnv *jvmti, jobject object) {
    std::vector<jobject> objects;
    objects.push_back(object);
    std::vector<jlong> result;
    jvmtiError err = estimateObjectsSizes(env, jvmti, objects, result);
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
    jvmtiError err = estimateObjectsSizes(env, jvmti, javaObjects, result);
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate objects size");
        return env->NewLongArray(0);
    }

    return toJavaArray(env, result);
}
