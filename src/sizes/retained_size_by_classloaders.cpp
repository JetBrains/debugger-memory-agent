// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <vector>
#include "retained_size_by_classloaders.h"
#include <algorithm>
#include <numeric>


jvmtiError RetainedSizeByClassLoadersAction::tagObjectsByClassLoader(jclass *classes, jint* cnt, jobject classLoader, jsize classLoaderIndex) {
    jvmtiError err = JVMTI_ERROR_NONE;
    std::vector<jclass> filteredClasses;
    for (jsize i = 0; i < *cnt; i++) {
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

jlong RetainedSizeByClassLoadersAction::tagOtherObjects(jclass *classes, jint* cnt, jobjectArray classLoadersArray) {
    jvmtiError err = JVMTI_ERROR_NONE;
    std::vector<jclass> filteredClasses;
    for (jsize i = 0; i < *cnt; i++) {
        jobject rootClassLoader = getClassLoader(env, classes[i]);
        if (!env->IsSameObject(rootClassLoader, NULL)) {
            for (jsize j = 0; j < env->GetArrayLength(classLoadersArray); j++) {
                if (isEqual(env, env->GetObjectArrayElement(classLoadersArray, j), rootClassLoader)) {
                    filteredClasses.emplace_back(classes[i]);
                    break;
                }
            }
        }
    }

    std::vector<jclass> nonfilteredClasses;
    for (jsize i = 0; i < *cnt; i++) {
        if (std::find(filteredClasses.begin(), filteredClasses.end(), classes[i]) == filteredClasses.end()) {
            nonfilteredClasses.emplace_back(classes[i]);
        }
    }
    std::cout << "SIZE: " << nonfilteredClasses.size() << std::endl;

    err = tagObjectsOfClassLoaderClasses(toJavaArray(env, nonfilteredClasses), env->GetArrayLength(classLoadersArray));
}

jlong RetainedSizeByClassLoadersAction::getHeapSize(){
    jlong totalSize = 0;
    IterateThroughHeap(0, nullptr, countHeapSize, &totalSize,
                       "calculate heap size");
    return totalSize;
}

RetainedSizeByClassLoadersAction::RetainedSizeByClassLoadersAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object)
        : RetainedSizeAction(env, jvmti, object) {

}

jvmtiError RetainedSizeByClassLoadersAction::getRetainedSizeByClassLoaders(jobjectArray classLoadersArray,
                                                                           std::vector <jlong> &result) {
    jvmtiError err = JVMTI_ERROR_NONE;
    jclass *classes;
    jint cnt;
    err = jvmti->GetLoadedClasses(&cnt, &classes);
    std::cout << cnt << std::endl;
    for (jsize i = 0; i < env->GetArrayLength(classLoadersArray); i++) {
        jvmtiError err = tagObjectsByClassLoader(classes, &cnt, env->GetObjectArrayElement(classLoadersArray, i), i);
    }
    tagOtherObjects(classes, &cnt, classLoadersArray);

    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    err = tagHeap();
    if (!isOk(err)) return err;
    if (shouldStopExecution()) return MEMORY_AGENT_INTERRUPTED_ERROR;

    result.resize(env->GetArrayLength(classLoadersArray) + 1);
    err = IterateThroughHeap(JVMTI_HEAP_FILTER_UNTAGGED, nullptr, visitObject, result.data(),
                             "calculate retained size");
    return err;
}

jlongArray RetainedSizeByClassLoadersAction::executeOperation(jobjectArray classLoadersArray) {
    std::vector <jlong> result;
    jvmtiError err = getRetainedSizeByClassLoaders(classLoadersArray, result);
    if (!isOk(err)) {
        handleError(jvmti, err, "Could not estimate retained size by classLoaders");
        return env->NewLongArray(0);
    }
    std::cout << "HEAP SIZE: " << getHeapSize() << std::endl;
    std::cout << "CLASSLOADERS CLASSES: " << std::accumulate(result.begin(), result.end()-1, 0) <<std::endl;
    std::cout << "OTHER CLASSES: " << result.back() <<std::endl;
    result.pop_back();
    return toJavaArray(env, result);
}