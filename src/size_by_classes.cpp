// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.
#include <jni.h>
#include <vector>
#include <jvmti.h>
#include <iostream>
#include <cstring>
#include "utils.h"

static jint cbHeapObjectIterationCallback(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (classTag != 0) {
        reinterpret_cast<jlong *>(userData)[classTag - 1] += size;
    }
    return JVMTI_VISIT_OBJECTS;
}

jlongArray getSizes(jobjectArray classesArray, jvmtiEnv *jvmti, JNIEnv *env) {
    jvmtiError err;
    jsize classesCount = env->GetArrayLength(classesArray);
    jlongArray result = env->NewLongArray(classesCount);
    auto sizes = new jlong[classesCount];
    for (jsize i = 0; i < classesCount; i++) {
        sizes[i] = 0;
        jobject classObject = env->GetObjectArrayElement(classesArray, i);
        err = jvmti->SetTag(classObject, i + 1);
        handleError(jvmti, err, "could not set tag for class object");
    }

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));

    cb.heap_iteration_callback = reinterpret_cast<jvmtiHeapIterationCallback>(&cbHeapObjectIterationCallback);

    jvmti->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_UNTAGGED, nullptr, &cb, sizes);
    env->SetLongArrayRegion(result, 0, classesCount, sizes);

    for (jsize i = 0; i < classesCount; i++) {
        jvmti->SetTag(env->GetObjectArrayElement(classesArray, i), 0);
    }

    return result;
}
