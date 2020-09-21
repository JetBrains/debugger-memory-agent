// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "../roots/paths_to_closest_gc_roots.h"
#include "objects_of_class_in_heap.h"

#define CLASS_TAG 1
#define REFERENCE_CLASS_TAG 2
#define WEAK_SOFT_REACHABLE_TAG 3
#define STRONG_REACHABLE_TAG 4
#define OBJECT_OF_CLASS_TAG 5

jint JNICALL findReachableObjectsOfClass(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                                        jlong *referrerTagPtr, jint length, void *userData) {
    if (*tagPtr == REFERENCE_CLASS_TAG || *tagPtr == CLASS_TAG) {
        return JVMTI_VISIT_OBJECTS;
    } else if (classTag == REFERENCE_CLASS_TAG) {
        *tagPtr = WEAK_SOFT_REACHABLE_TAG; // tag soft/weak/phantom reference
    } else if (*tagPtr == 0) {
        if (referrerTagPtr != nullptr && *referrerTagPtr == WEAK_SOFT_REACHABLE_TAG) {
            *tagPtr = WEAK_SOFT_REACHABLE_TAG;
        } else {
            *tagPtr = STRONG_REACHABLE_TAG;
        }
    } else if (*tagPtr == WEAK_SOFT_REACHABLE_TAG) {
        if (referrerTagPtr == nullptr || *referrerTagPtr != WEAK_SOFT_REACHABLE_TAG) {
            *tagPtr = STRONG_REACHABLE_TAG;
        }
    }

    if (classTag == CLASS_TAG && *tagPtr == STRONG_REACHABLE_TAG) {
        *tagPtr = OBJECT_OF_CLASS_TAG;
        if (*reinterpret_cast<bool *>(userData)) {
            return JVMTI_VISIT_ABORT;
        }
    }
    return JVMTI_VISIT_OBJECTS;
}


static jvmtiError getReachableObjectsOfClass(JNIEnv *env, jvmtiEnv *jvmti, jobject startObject, jobject classObject, std::vector<jobject> &result, bool findFirst) {
    setTagsForReferences(env, jvmti, REFERENCE_CLASS_TAG);

    jclass *classes;
    jint cnt;
    jvmtiError err = jvmti->GetLoadedClasses(&cnt, &classes);
    if (err != JVMTI_ERROR_NONE) return err;

    jclass langClass = env->FindClass("java/lang/Class");
    jmethodID isAssignableFrom = env->GetMethodID(langClass, "isAssignableFrom", "(Ljava/lang/Class;)Z");

    for (int i = 0; i < cnt; i++) {
        if (env->CallBooleanMethod(classObject, isAssignableFrom, classes[i])) {
            err = jvmti->SetTag(classes[i], CLASS_TAG);
            if (err != JVMTI_ERROR_NONE) return err;
        }
    }

    jvmtiHeapCallbacks cb;
    std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
    cb.heap_reference_callback = findReachableObjectsOfClass;
    err = jvmti->FollowReferences(0, nullptr, startObject, &cb, &findFirst);
    if (err != JVMTI_ERROR_NONE) return err;

    return getObjectsByTags(jvmti, std::vector<jlong>{OBJECT_OF_CLASS_TAG}, result);
}

GetFirstReachableObjectOfClassAction::GetFirstReachableObjectOfClassAction(JNIEnv *env, jvmtiEnv *jvmti) : MemoryAgentTimedAction(env, jvmti) {

}

jobject GetFirstReachableObjectOfClassAction::executeOperation(jobject startObject, jobject classObject) {
    std::vector<jobject> result;
    jvmtiError err = getReachableObjectsOfClass(env, jvmti, startObject, classObject, result, true);
    handleError(jvmti, err, "Couldn't check class reachability");
    return result.empty() ? nullptr : result[0];
}

jvmtiError GetFirstReachableObjectOfClassAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}

GetAllReachableObjectsOfClassAction::GetAllReachableObjectsOfClassAction(JNIEnv *env, jvmtiEnv *jvmti) : MemoryAgentTimedAction(env, jvmti) {

}

jobjectArray GetAllReachableObjectsOfClassAction::executeOperation(jobject startObject, jobject classObject) {
    std::vector<jobject> reachableObjects;
    jvmtiError err = getReachableObjectsOfClass(env, jvmti, startObject, classObject, reachableObjects, false);
    handleError(jvmti, err, "Couldn't get reachable objects of class");
    jclass langObject = env->FindClass("java/lang/Object");
    jobjectArray resultArray = env->NewObjectArray(reachableObjects.size(), langObject, nullptr);

    for (jsize i = 0; i < reachableObjects.size(); ++i) {
        env->SetObjectArrayElement(resultArray, i, reachableObjects[i]);
    }

    return resultArray;
}

jvmtiError GetAllReachableObjectsOfClassAction::cleanHeap() {
    return removeAllTagsFromHeap(jvmti, nullptr);
}
