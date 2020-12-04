// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_TIMED_ACTION_H
#define MEMORY_AGENT_TIMED_ACTION_H

#include <chrono>
#include <cstring>
#include "jni.h"
#include "jvmti.h"
#include "log.h"
#include "utils.h"
#include "cancellation_manager.h"

#define MEMORY_AGENT_INTERRUPTED_ERROR static_cast<jvmtiError>(999)

struct CallbackWrapperData {
    CallbackWrapperData(void *callback, void *userData, const CancellationManager *manager) :
            callback(callback), userData(userData), manager(manager) {

    }

    const CancellationManager *manager;
    void *callback;
    void *userData;
};

enum class ErrorCode {
    OK = 0,
    TIMEOUT = 1,
    CANCELLED = 2
};

template<typename RESULT_TYPE, typename... ARGS_TYPES>
class MemoryAgentTimedAction : public CancellationManager {
public:
    MemoryAgentTimedAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);
    virtual ~MemoryAgentTimedAction() = default;

    jobjectArray run(ARGS_TYPES... args);

protected:
    virtual RESULT_TYPE executeOperation(ARGS_TYPES... args) = 0;
    virtual jvmtiError cleanHeap() = 0;

    static jint JNICALL followReferencesCallbackWrapper(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                                                        jlong *referrerTagPtr, jint length, void *userData);

    static jint JNICALL iterateThroughHeapCallbackWrapper(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

    jvmtiError FollowReferences(jint heapFilter, jclass klass, jobject initialObject,
                                jvmtiHeapReferenceCallback callback, void *userData,
                                const char *debugMessage=nullptr) const;

    jvmtiError IterateThroughHeap(jint heapFilter, jclass klass, jvmtiHeapIterationCallback callback,
                                  void *userData, const char *debugMessage=nullptr) const;
private:
    ErrorCode getErrorCode() const;

protected:
    JNIEnv *env;
    jvmtiEnv *jvmti;
};


#include "timed_action.hpp"


#endif //MEMORY_AGENT_TIMED_ACTION_H
