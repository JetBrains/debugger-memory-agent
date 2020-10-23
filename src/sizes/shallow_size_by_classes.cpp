// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <memory>
#include "shallow_size_by_classes.h"
#include "sizes_tags.h"
#include "retained_size_action.h"

static jint JNICALL calculateShallowSize(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
    ClassTag *pClassTag = tagToClassTagPointer(classTag);
    if (pClassTag) {
        for (query_size_t id : pClassTag->ids) {
            reinterpret_cast<jlong *>(userData)[id] += size;
        }
    }
    return JVMTI_VISIT_OBJECTS;
}

ShallowSizeByClassesAction::ShallowSizeByClassesAction(JNIEnv *env, jvmtiEnv *jvmti, jobject cancellationFileName) : MemoryAgentTimedAction(env, jvmti, cancellationFileName) {

}

void ShallowSizeByClassesAction::tagClasses(jobjectArray classesArray) {
    for (jsize i = 0; i < env->GetArrayLength(classesArray); i++) {
        jobject classObject = env->GetObjectArrayElement(classesArray, i);
        jvmtiError err = tagClassAndItsInheritors(env, jvmti, classObject,  [i](jlong oldTag) -> jlong {
            ClassTag *classTag = tagToClassTagPointer(oldTag);
            if (classTag != nullptr) {
                classTag->ids.push_back(i);
            } else {
                return pointerToTag(ClassTag::create(static_cast<query_size_t>(i)));
            }

            return 0;
        });
        handleError(jvmti, err, "could not set getTag for class object");
    }
}

jlongArray ShallowSizeByClassesAction::executeOperation(jobjectArray classesArray) {
    jsize classesCount = env->GetArrayLength(classesArray);
    jlongArray result = env->NewLongArray(classesCount);
    auto sizes = new jlong[classesCount];
    std::memset(sizes, 0, sizeof(jlong) * classesCount);
    tagClasses(classesArray);

    if (shouldStopAction()) return env->NewLongArray(0);

    IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_UNTAGGED, nullptr, calculateShallowSize, sizes);
    env->SetLongArrayRegion(result, 0, classesCount, sizes);
    return result;
}

jvmtiError ShallowSizeByClassesAction::cleanHeap() {
    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_iteration_callback = clearTag;
    jvmtiError err =  this->jvmti->IterateThroughHeap(0, nullptr, &cb, nullptr);

    if (sizesTagBalance != 0) {
        fatal("MEMORY LEAK FOUND!");
    }

    return err;
}
