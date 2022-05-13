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

jvmtiError RetainedSizeByClassLoadersAction::getSizeByClassLoader(jobject classLoader, jlong *size) {
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
    auto sizes = retainedSizeByClasses.run(toJavaArray(env, filteredClasses));
    for (jsize i = 0; i < env->GetArrayLength(sizes); i++) {
        *size +=  reinterpret_cast<jlong>(env->GetObjectArrayElement(sizes, i));
    }
    return err;
}

RetainedSizeByClassLoadersAction::RetainedSizeByClassLoadersAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object)
        : RetainedSizeAction(env, jvmti, object), retainedSizeByClasses(env, jvmti, object) {

}

jvmtiError RetainedSizeByClassLoadersAction::getRetainedSizeByClassLoaders(jobjectArray classLoadersArray,
                                                                           std::vector <jlong> &result) {
    jvmtiError err = JVMTI_ERROR_NONE;
    result.resize(env->GetArrayLength(classLoadersArray));
    for (jsize i = 0; i < env->GetArrayLength(classLoadersArray); i++) {
        jlong res = 0;
        jvmtiError err = getSizeByClassLoader(env->GetObjectArrayElement(classLoadersArray, i), &res);
        result[i] = res;
    }
    return err;
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