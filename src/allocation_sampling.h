// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_ALLOCATION_SAMPLING_H
#define MEMORY_AGENT_ALLOCATION_SAMPLING_H

#include <vector>
#include "jni.h"
#include "jvmti.h"

class ListenersHolder {
public:
    ListenersHolder() = default;
    ~ListenersHolder() = default;

    void addListener(JNIEnv *env, jobject listener);
    void notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size);
    void clear(JNIEnv *env);

private:
    void setOnAllocationMethod(JNIEnv *env);

private:
    static const char *allocationListenerClassName;
    static const char *onAllocationMethodName;
    static const char *onAllocationMethodSignature;

    std::vector<jobject> listeners;
    jmethodID onAllocationMethod = nullptr;
};

extern ListenersHolder listenersHolder;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size);


#endif //MEMORY_AGENT_ALLOCATION_SAMPLING_H
