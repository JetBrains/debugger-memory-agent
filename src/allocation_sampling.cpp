// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "allocation_sampling.h"
#include "utils.h"

const char *AllocationListener::allocationListenerClassName = "com/intellij/memory/agent/AllocationListener";
const char *AllocationListener::onAllocationMethodName = "onAllocation";
const char *AllocationListener::onAllocationMethodSignature = "(Ljava/lang/Thread;Ljava/lang/Object;Ljava/lang/Class;J)V";

ListenersHolder listenersHolder;

extern "C" JNIEXPORT void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
                                                     jobject object, jclass klass, jlong size) {
    listenersHolder.notifyAll(env, thread, object, klass, size);
}

size_t ListenersHolder::addListener(JNIEnv *env, jobject listener, const std::vector<jobject> &trackedClasses) {
    listeners.emplace_back(env, listener, trackedClasses);
    return listeners.size() - 1;
}

void ListenersHolder::notifyAll(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size) {
    for (AllocationListener listener : listeners) {
        if (listener.shouldBeNotified(env, klass)) {
            listener.notify(env, thread, object, klass, size);
        }
    }
}

void ListenersHolder::removeListener(JNIEnv *env, size_t index) {
    auto it = listeners.begin();
    for (size_t i = 0; it != listeners.end() && i < index; ++it, ++i);
    it->deleteReferences(env);
    listeners.erase(it);
}

void AllocationListener::notify(JNIEnv *env, jthread thread, jobject object, jclass klass, jlong size) {
    env->CallVoidMethod(listener, onAllocationMethod, thread, object, klass, size);
}

AllocationListener::AllocationListener(JNIEnv *env, jobject listener, const std::vector<jobject> &classes) :
    listener(env->NewGlobalRef(listener)) {
    trackedClasses.reserve(classes.size());
    for (jobject klass : classes) {
        trackedClasses.push_back(env->NewGlobalRef(klass));
    }

    jclass listenerClass = env->FindClass(allocationListenerClassName);
    onAllocationMethod = env->GetMethodID(listenerClass, onAllocationMethodName, onAllocationMethodSignature);
    isAssignableFrom = getIsAssignableFromMethod(env);
}

bool AllocationListener::shouldBeNotified(JNIEnv *env, jclass klass) {
    if (trackedClasses.empty()) {
        return true;
    }

    for (jobject trackedClass : trackedClasses) {
        if (env->CallBooleanMethod(trackedClass, isAssignableFrom, klass)) {
            return true;
        }
    }

    return false;
}

void AllocationListener::deleteReferences(JNIEnv *env) {
    env->DeleteGlobalRef(listener);

    for (jobject klass : trackedClasses) {
        env->DeleteGlobalRef(klass);
    }

    trackedClasses.clear();
}
