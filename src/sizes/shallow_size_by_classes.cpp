// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <memory>
#include "shallow_size_by_classes.h"

static jint JNICALL calculateShallowSize(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    if (classTag != 0) {
        reinterpret_cast<jlong *>(userData)[classTag - 1] += size;
    }
    return JVMTI_VISIT_OBJECTS;
}

ShallowSizeByClassesAction::ShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti) : MemoryAgentTimedAction(env, jvmti) {

}

void  ShallowSizeByClassesAction::tagClasses(jobjectArray classesArray) {
    for (jsize i = 0; i < env->GetArrayLength(classesArray); i++) {
        jobject classObject = env->GetObjectArrayElement(classesArray, i);
        jvmtiError err = jvmti->SetTag(classObject, i + 1);
        handleError(jvmti, err, "could not set getTag for class object");
    }
}

jlongArray ShallowSizeByClassesAction::executeOperation(jobjectArray classesArray) {
    jsize classesCount = env->GetArrayLength(classesArray);
    jlongArray result = env->NewLongArray(classesCount);
    auto sizes = new jlong[classesCount];
    std::memset(sizes, 0, sizeof(jlong) * classesCount);
    tagClasses(classesArray);

    if (shouldStopExecution()) return env->NewLongArray(0);

    IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_UNTAGGED, nullptr, calculateShallowSize, sizes);
    env->SetLongArrayRegion(result, 0, classesCount, sizes);
    return result;
}

jvmtiError ShallowSizeByClassesAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}
