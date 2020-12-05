// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "allocation_sampling.h"

const char *ListenersHolder::allocationListenerClassName = "com/intellij/memory/agent/AllocationListener";
const char *ListenersHolder::onAllocationMethodName = "onAllocation";
const char *ListenersHolder::onAllocationMethodSignature = "(Ljava/lang/Thread;Ljava/lang/Object;Ljava/lang/Class;J)V";

ListenersHolder listenersHolder;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size) {
    listenersHolder.notifyAll(env, thread, object, klass, size);
}

void ListenersHolder::addListener(JNIEnv *env, jobject listener) {
    setOnAllocationMethod(env);
    listeners.push_back(env->NewGlobalRef(listener));
}

void ListenersHolder::notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size) {
    if (onAllocationMethod) {
        for (jobject listener : listeners) {
            env->CallVoidMethod(listener, onAllocationMethod, thread, object, klass, size);
        }
    }
}

void ListenersHolder::setOnAllocationMethod(JNIEnv *env) {
    if (!onAllocationMethod) {
        jclass listenerClass = env->FindClass(allocationListenerClassName);
        onAllocationMethod = env->GetMethodID(listenerClass, onAllocationMethodName, onAllocationMethodSignature);
    }
}

void ListenersHolder::clear(JNIEnv *env) {
    for (jobject listener : listeners) {
        env->DeleteGlobalRef(listener);
    }
}
