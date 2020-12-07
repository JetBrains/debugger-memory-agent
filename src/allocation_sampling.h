// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_ALLOCATION_SAMPLING_H
#define MEMORY_AGENT_ALLOCATION_SAMPLING_H

#include <list>
#include <vector>

#include "jni.h"
#include "jvmti.h"

class AllocationListener {
public:
    AllocationListener(JNIEnv *env, jobject listener, const std::vector<jobject> &classes);
    ~AllocationListener() = default;

    void notify(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size);
    bool shouldBeNotified(JNIEnv *env, jclass klass);

    void deleteReferences(JNIEnv *env);

private:
    static const char *allocationListenerClassName;
    static const char *onAllocationMethodName;
    static const char *onAllocationMethodSignature;

    jobject listener;
    jmethodID onAllocationMethod;
    jmethodID isAssignableFrom;
    std::vector<jobject> trackedClasses;
};

class ListenersHolder {
public:
    ListenersHolder() = default;
    ~ListenersHolder() = default;

    size_t addListener(JNIEnv *env, jobject listener, const std::vector<jobject> &trackedClasses);
    void removeListener(JNIEnv *env, size_t index);
    void notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size);

private:
    std::list<AllocationListener> listeners;
};

extern ListenersHolder listenersHolder;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size);


#endif //MEMORY_AGENT_ALLOCATION_SAMPLING_H
