// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_by_classloaders.h"


jvmtiError RetainedSizeByClassLoadersAction::tagObjectsByClassLoader(jobject classLoader, jsize classLoaderIndex) {
    jvmtiError err = JVMTI_ERROR_NONE;
    jclass *classes;
    jint cnt;
    err = jvmti->GetLoadedClasses(&cnt, &classes);
    if (!isOk(err)) return err;

    std::vector<jclass> filteredClasses;
    for (jsize i = 0; i < cnt; i++) {
        jobject rootClassLoader = getClassLoader(env, classes[i]);
        if (!env->IsSameObject(rootClassLoader, NULL)) {
            if (isEqual(env, classLoader, rootClassLoader)) {
                filteredClasses.emplace_back(classes[i]);
            }
        }
    }

    err = tagObjectsOfClassLoaderClasses(toJavaArray(env, filteredClasses), classLoaderIndex);
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

    result.resize(env->GetArrayLength(classLoadersArray));
    return IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(),
                              "calculate retained sizes");
}

jlongArray RetainedSizeByClassLoadersAction::executeOperation(jobjectArray classLoadersArray) {
    std::vector <jlong> result;
    jvmtiError err = getRetainedSizeByClassLoaders(classLoadersArray, result);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classLoaders");
        return env->NewLongArray(0);
    }
    return toJavaArray(env, result);
}