// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <cstring>
#include <unordered_set>
#include "objects_size.h"
#include "utils.h"
#include "log.h"

static jlong tagBalance = 0;

typedef struct Tag {
private:
    explicit Tag() = default;

public:
    std::unordered_map<jint, uint8_t> states;

    static Tag *create() {
        ++tagBalance;
        return new Tag();
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

jint JNICALL visitReference(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                    jlong referrerClassTag, jlong size, jlong *tagPtr,
                    jlong *referrerTagPtr, jint length, void *_) { // NOLINT(readability-non-const-parameter)

    bool alreadyVisited = *tagPtr != 0;
    if (!alreadyVisited) {
        *tagPtr = pointerToTag(Tag::create());
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
            refereeTag->states[entry.first] = updateState(currentState, entry.second);
        }
    }

    return JVMTI_VISIT_OBJECTS;
}

#pragma clang diagnostic pop

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    Tag *tag = tagToPointer(*tagPtr);
    for (const auto &entry: tag->states) {
        if (isRetained(entry.second)) {
            reinterpret_cast<jlong *>(userData)[entry.first] += size;
        }
    }

    delete tag;
    *tagPtr = 0;
    return JVMTI_ITERATION_CONTINUE;
}

jvmtiError estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, std::vector<jobject> &objects,
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
        jlong oldTag = 0;
        err = jvmti->GetTag(object, &oldTag);
        if (err != JVMTI_ERROR_NONE) return err;
        Tag *tag = oldTag == 0 ? Tag::create() : tagToPointer(oldTag);
        tag->states[i] = create_state(true, true, false);
        err = jvmti->SetTag(objects[i], pointerToTag(tag));
        if (err != JVMTI_ERROR_NONE) return err;
    }

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
    if (tagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }
    return err;
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
