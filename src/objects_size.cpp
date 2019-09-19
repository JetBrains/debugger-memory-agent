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

    int8_t class_id = -1;

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

    bool alreadyVisited = *tagPtr != 0 && tagToPointer(*tagPtr)->states.empty();
    if (*tagPtr == 0) {
        *tagPtr = pointerToTag(Tag::create());
    }
    
    if (classTag != 0) {
        int8_t class_id = tagToPointer(classTag)->class_id;
        if (class_id != -1) {
            uint8_t &state = tagToPointer(*tagPtr)->states[class_id];
            // start, in subtree, not reachable from outside
            state = (state | 3u) & ~(1u << 2u);
        }
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

        // TODO do not allow nested memory counting.
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

jvmtiError estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti,
                                std::vector<jlong> &result) {
    jvmtiError err;

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = reinterpret_cast<jvmtiHeapReferenceCallback>(&visitReference);
    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&visitObject);
    debug("tag heap");
    err = jvmti->FollowReferences(0, nullptr, nullptr, &cb, nullptr);
    if (err != JVMTI_ERROR_NONE) return err;
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
//    jvmtiError err = estimateObjectsSizes(env, jvmti, objects, result);
//    if (err != JVMTI_ERROR_NONE) {
//        handleError(jvmti, err, "Could not estimate object size");
//        return -1;
//    }
    if (result.size() != 1) {
        fatal("Unexpected result format: vector with one element expected.");
        return -1;
    }

    return result[0];
}

jlongArray estimateObjectsSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray objects) {
    debug("start estimate objects sizes");
    debug("convert java array to vector");
    std::vector<std::vector<jobject>> objects;
    fromJavaArray(env, arrayOfArrays, objects);
    std::vector<jlong> result;
//    jvmtiError err = estimateObjectsSizes(env, jvmti, objects, result);
//    if (err != JVMTI_ERROR_NONE) {
//        handleError(jvmti, err, "Could not estimate objects size");
//        return env->NewLongArray(0);
//    }

    return toJavaArray(env, result);
}

void markClass(jvmtiEnv *jvmti, jobject clazz, uint8_t classId) {
    jvmtiError err;
    jlong oldTag = 0;
    err = jvmti->GetTag(clazz, &oldTag);
    if (err != JVMTI_ERROR_NONE) handleError(jvmti, err, "cant get tag");
    Tag *tag = oldTag == 0 ? Tag::create() : tagToPointer(oldTag);
    tag->class_id = classId;
    err = jvmti->SetTag(clazz, pointerToTag(tag));
    if (err != JVMTI_ERROR_NONE) handleError(jvmti, err, "cant set tag");
}

static bool isProperClassLoaderName(const std::string &name) {
    info(name.c_str());
    return name.find("PluginClassLoader") != std::string::npos;
}

std::string getToString(JNIEnv *env, const jobject &classLoader) {
//    return "PluginClassLoader";
    jclass classLoaderClass = env->GetObjectClass(classLoader);
    if (classLoaderClass == nullptr) {
        warn("null class loader class");
        return "";
    }

    jmethodID methodId = env->GetMethodID(classLoaderClass, "toString", "()Ljava/lang/String;");
    if (methodId == nullptr) {
        error("no toString!");
        return "";
    }

    auto string = (jstring)env->CallObjectMethod(classLoader, methodId);

    const char *chars = env->GetStringUTFChars(string, nullptr);
    std::string classLoaderName(chars);
    env->ReleaseStringUTFChars(string, chars);
    return classLoaderName;
}

static std::vector<jobject> tagClasses(JNIEnv *env, jvmtiEnv *jvmti, jint count, jclass *classes) {
    auto result = std::vector<jobject>();
    if (count == 0) {
        return result;
    }

    std::vector<jobject> badClassLoaders;

    for (jsize i = 0; i < count; ++i) {
        jclass clazz = classes[i];

        jobject classLoader;
        jvmtiError err = jvmti->GetClassLoader(clazz, &classLoader);
        handleError(jvmti, err, "failed to get class loader");

        if (classLoader == nullptr) {
            continue;
        }

        jlong tag;
        jvmti->GetTag(classLoader, &tag);
        if (tag != 0) {
            if (tag != -1) {
                markClass(jvmti, clazz, tag - 1);
            }
            continue;
        }

        std::string classLoaderName = getToString(env, classLoader);

        if (isProperClassLoaderName(classLoaderName)) {
            result.push_back(classLoader);
            markClass(jvmti, clazz, result.size() - 1);
            jvmti->SetTag(classLoader, result.size());
        }
        else {
            badClassLoaders.push_back(classLoader);
            jvmti->SetTag(classLoader, -1);
        }
    }

    for (const auto &object : result) jvmti->SetTag(object, 0);
    for (const auto &object : badClassLoaders) jvmti->SetTag(object, 0);

    return result;
}

jobjectArray wrapAnswer(JNIEnv *env, const std::vector<jobject> &classLoaders, std::vector<jlong> &sizes) {
    jclass langObject = env->FindClass("java/lang/Object");

    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);

    jobjectArray classLoadersArray = env->NewObjectArray(classLoaders.size(), langObject, nullptr);
    for (size_t i = 0; i < classLoaders.size(); ++i) {
        env->SetObjectArrayElement(classLoadersArray, i, classLoaders[i]);
    }
    env->SetObjectArrayElement(result, 0, classLoadersArray);

    jlongArray sizeArray = toJavaArray(env, sizes);
    env->SetObjectArrayElement(result, 1, sizeArray);

    return result;
}

jobjectArray estimateObjectsSizesByPluginClassLoaders(JNIEnv *env, jvmtiEnv *jvmti) {
    jvmtiError err;

    tagBalance = 0;

    info("Get all classes");
    jint classes_count;
    jclass *classes_ptr;
    err = jvmti->GetLoadedClasses(&classes_count, &classes_ptr);
    handleError(jvmti, err, "Could not get list of classes");
    info("Mark classes");
    const std::vector<jobject> &classLoaders = tagClasses(env, jvmti, classes_count, classes_ptr);
    info("Class loaders number:");
    info(std::to_string(classLoaders.size()).c_str());
    info("Traverse references");
    std::vector<jlong> result(classLoaders.size());
    estimateObjectsSizes(env, jvmti, result);
    info("Done");
    if (err != JVMTI_ERROR_NONE) {
        handleError(jvmti, err, "Could not estimate objects size");
        return env->NewObjectArray(0, env->FindClass("java/lang/Object"), nullptr);
    }

    return wrapAnswer(env, classLoaders, result);

}