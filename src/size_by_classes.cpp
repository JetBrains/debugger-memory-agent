// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <jni.h>
#include <vector>
#include <jvmti.h>
#include <cstring>
#include "utils.h"

static jint JNICALL cbHeapObjectIterationCallback(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (classTag != 0) {
        reinterpret_cast<jlong *>(userData)[classTag - 1] += size;
    }
    return JVMTI_VISIT_OBJECTS;
}

static void tagClasses(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray, bool setTagToZero = false) {
    for (jsize i = 0; i < env->GetArrayLength(classesArray); i++) {
        jobject classObject = env->GetObjectArrayElement(classesArray, i);
        jvmtiError err = jvmti->SetTag(classObject, setTagToZero ? 0 : i + 1);
        handleError(jvmti, err, "could not set tag for class object");
    }
}

jlongArray getSizes(JNIEnv *env, jvmtiEnv *jvmti, jobjectArray classesArray) {
    jsize classesCount = env->GetArrayLength(classesArray);
    jlongArray result = env->NewLongArray(classesCount);
    auto sizes = new jlong[classesCount];
    std::memset(sizes, 0, sizeof(jlong) * classesCount);

    tagClasses(env, jvmti, classesArray);

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));

    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&cbHeapObjectIterationCallback);

    jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_UNTAGGED, nullptr, &cb, sizes);
    env->SetLongArrayRegion(result, 0, classesCount, sizes);

    tagClasses(env, jvmti, classesArray, true);

    return result;
}
