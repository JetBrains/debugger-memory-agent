// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_by_classloaders.h"


const jlong GC_ROOT_TAG = 17;
const jlong FILTERED_OBJECT = 15;

jvmtiIterationControl JNICALL tagRootsCallback(jvmtiHeapRootKind root_kind,
                                               jlong class_tag,
                                               jlong size,
                                               jlong *tag_ptr,
                                               void *user_data) {
    if (root_kind != JVMTI_HEAP_ROOT_JNI_GLOBAL && root_kind != JVMTI_HEAP_ROOT_JNI_LOCAL) {
        *tag_ptr = GC_ROOT_TAG;
    }
    return JVMTI_ITERATION_IGNORE;
}

jvmtiIterationControl JNICALL tagObjectCallback (jvmtiObjectReferenceKind reference_kind,
                                               jlong class_tag,
                                               jlong size,
                                               jlong *tag_ptr,
                                               jlong referrer_tag,
                                               jint referrer_index,
                                               void *user_data){
    *tag_ptr = FILTERED_OBJECT;
    return JVMTI_ITERATION_CONTINUE;
}

jvmtiError RetainedSizeByClassLoadersAction::tagObjectsByClassLoader(jobject classLoader, jsize classLoaderIndex) {
    jvmtiError err = JVMTI_ERROR_NONE;
    err = jvmti->IterateOverReachableObjects(tagRootsCallback, NULL, NULL, NULL);
    jclass *classes;
    jint cnt;
    err = jvmti->GetLoadedClasses(&cnt, &classes);
    if (!isOk(err)) return err;
    removeAllTagsFromHeap(jvmti, nullptr);

    std::vector<jclass> filteredClasses;
    for (jsize i = 0; i < cnt; i++) {
        jobject rootClassLoader = getClassLoader(env, classes[i]);
        if (!env->IsSameObject(rootClassLoader, NULL)) {
            if (isEqual(env, classLoader, rootClassLoader)) {
                filteredClasses.emplace_back(classes[i]);
            }
        }
    }
    removeAllTagsFromHeap(jvmti, nullptr);
    std::cout << "GET CLASSES" << std::endl;

    err = tagObjectsOfClassLoaderClasses(toJavaArray(env, filteredClasses), classLoaderIndex);
    std::cout << "TAGGED CLASSES" << std::endl;
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    return err;
}

RetainedSizeByClassLoadersAction::RetainedSizeByClassLoadersAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object)
        : RetainedSizeAction(env, jvmti, object) {

}

jvmtiError RetainedSizeByClassLoadersAction::getRetainedSizeByClassLoaders(jobjectArray classLoadersArray,
                                                                           std::vector <jlong> &result) {
    jvmtiError err = JVMTI_ERROR_NONE;
    for (jsize i = 0; i < env->GetArrayLength(classLoadersArray); i++) {
        jvmtiError err = tagObjectsByClassLoader(env->GetObjectArrayElement(classLoadersArray, i), i);
    }
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = tagHeap();
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;
    std::cout << "TAGGED HEAP" << std::endl;

    result.resize(env->GetArrayLength(classLoadersArray));
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(),
                              "calculate retained sizes");
    return err;
}

jlongArray RetainedSizeByClassLoadersAction::executeOperation(jobjectArray classLoadersArray) {
    std::vector <jlong> result;
    jvmtiError err = getRetainedSizeByClassLoaders(classLoadersArray, result);
    std::cout << "GET SIZES" << std::endl;
    removeAllTagsFromHeap(jvmti, nullptr);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classLoaders");
        return env->NewLongArray(0);
    }
    return toJavaArray(env, result);
}