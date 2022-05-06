// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_by_classloaders.h"


const jlong GC_ROOT_TAG = 17;

jvmtiIterationControl JNICALL putTagCallback(jvmtiHeapRootKind root_kind,
                                               jlong class_tag,
                                               jlong size,
                                               jlong *tag_ptr,
                                               void *user_data) {
    if (root_kind != JVMTI_HEAP_ROOT_JNI_GLOBAL && root_kind != JVMTI_HEAP_ROOT_JNI_LOCAL) {
        *tag_ptr = GC_ROOT_TAG;
    }
    return JVMTI_ITERATION_IGNORE;
}

jobjectArray RetainedSizeByClassLoadersAction::getFilteredRoots(jobject classLoader) {
    jvmti->IterateOverReachableObjects(putTagCallback, NULL, NULL, NULL);
    jint nroots;
    jobject *roots;
    jvmti->GetObjectsWithTags(1, &GC_ROOT_TAG, &nroots, &roots, NULL);
    removeAllTagsFromHeap(jvmti, nullptr);

    std::vector <jobject> filteredRoots;
    for (jsize i = 0; i < nroots; i++) {
        jobject rootClassLoader = getClassLoader(env, roots[i]);
        if (!env->IsSameObject(rootClassLoader, NULL)) {
            if (isEqual(env, classLoader, rootClassLoader)) {
                filteredRoots.push_back(roots[i]);
            }
        }
    }
    return toJavaArray(env, filteredRoots);
}

jvmtiError RetainedSizeByClassLoadersAction::tagRootLoadedClassesByClassLoaders(jobjectArray classLoadersArray) {
    debug("tag classes loaded by classloaders");
    for (jsize i = 0; i < env->GetArrayLength(classLoadersArray); i++) {
        jobjectArray filteredRoots = getFilteredRoots(env->GetObjectArrayElement(classLoadersArray, i));
        jvmtiError err = createTagsForClasses(this->env, this->jvmti, filteredRoots);
        if (err != JVMTI_ERROR_NONE) return err;
    }
    return IterateThroughHeap(0, nullptr, tagObjectOfTaggedClass, nullptr);
}

RetainedSizeByClassLoadersAction::RetainedSizeByClassLoadersAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object)
        : RetainedSizeAction(env, jvmti, object) {

}

jvmtiError RetainedSizeByClassLoadersAction::getRetainedSizeByClassLoaders(jobjectArray classLoadersArray,
                                                                           std::vector <jlong> &result) {
    jvmtiError err = tagRootLoadedClassesByClassLoaders(classLoadersArray);
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = tagHeap();
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    result.resize(env->GetArrayLength(classLoadersArray));
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(),
                              "calculate retained sizes");
}

jlongArray RetainedSizeByClassLoadersAction::executeOperation(jobjectArray classLoadersArray) {
    std::vector <jlong> result;
    jvmtiError err = getRetainedSizeByClassLoaders(classLoadersArray, result);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classloaders");
        return env->NewLongArray(0);
    }
    return toJavaArray(env, result);
}