// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_by_classes.h"
#include "sizes_tags.h"
#include "retained_size_action.h"

jint JNICALL visitObject(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    for (query_size_t i = 0; i < tag->array.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag->array[i];
        if (isRetained(info.state)) {
            reinterpret_cast<jlong *>(userData)[info.index] += size;
        }
    }

    return JVMTI_ITERATION_CONTINUE;
}


jint JNICALL visitObjectForShallowAndRetainedSize(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (*tagPtr == 0) {
        return JVMTI_ITERATION_CONTINUE;
    }

    Tag *tag = tagToPointer(*tagPtr);
    for (query_size_t i = 0; i < tag->array.getSize(); i++) {
        const TagInfoArray::TagInfo &info = tag->array[i];
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

RetainedSizeByClassesAction::RetainedSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName, jlong duration) : RetainedSizeAction(env, jvmti, cancellationFileName, duration) {

}

jvmtiError RetainedSizeByClassesAction::getRetainedSizeByClasses(jobjectArray classesArray, std::vector<jlong> &result) {
    jvmtiError err = tagObjectsOfClasses(classesArray);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = tagHeap();
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    result.resize(env->GetArrayLength(classesArray));
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(), "calculate retained sizes");
}

jlongArray RetainedSizeByClassesAction::executeOperation(jobjectArray classesArray) {
    std::vector<jlong> result;
    jvmtiError err = getRetainedSizeByClasses(classesArray, result);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
        return env->NewLongArray(0);
    }

    return toJavaArray(env, result);
}

RetainedAndShallowSizeByClassesAction::RetainedAndShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName, jlong duration) : RetainedSizeAction(env, jvmti, cancellationFileName, duration) {

}

jvmtiError RetainedAndShallowSizeByClassesAction::getShallowAndRetainedSizeByClasses(jobjectArray classesArray,
                                                                                     std::vector<jlong> &shallowSizes,
                                                                                     std::vector<jlong> &retainedSizes) {
    jvmtiError err = tagObjectsOfClasses(classesArray);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = tagHeap();
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    retainedSizes.resize(env->GetArrayLength(classesArray));
    shallowSizes.resize(env->GetArrayLength(classesArray));
    std::pair<jlong *, jlong *> arrays = std::make_pair(shallowSizes.data(), retainedSizes.data());
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObjectForShallowAndRetainedSize, &arrays, "calculate shallow and retained sizes");
}

jobjectArray RetainedAndShallowSizeByClassesAction::executeOperation(jobjectArray classesArray) {
    std::vector<jlong> shallowSizes;
    std::vector<jlong> retainedSizes;
    jvmtiError err = getShallowAndRetainedSizeByClasses(classesArray,shallowSizes, retainedSizes);

    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(2, langObject, nullptr);
    env->SetObjectArrayElement(result, 0, toJavaArray(env, shallowSizes));
    env->SetObjectArrayElement(result, 1, toJavaArray(env, retainedSizes));
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classes");
    }
    return result;
}
