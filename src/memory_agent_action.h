// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_MEMORY_AGENT_ACTION_H
#define MEMORY_AGENT_MEMORY_AGENT_ACTION_H

#include <chrono>
#include <cstring>
#include "jni.h"
#include "jvmti.h"
#include "log.h"
#include "utils.h"
#include "cancellation_checker.h"
#include "progress_manager.h"

#define MEMORY_AGENT_INTERRUPTED_ERROR static_cast<jvmtiError>(999)

template<typename RESULT_TYPE, typename... ARGS_TYPES>
class MemoryAgentAction : public CancellationChecker {
private:
    struct CallbackWrapperData {
        CallbackWrapperData(void *callback, void *userData, const CancellationChecker *cancellationChecker) :
                callback(callback), userData(userData), cancellationChecker(cancellationChecker) {

        }

        void *callback;
        void *userData;
        const CancellationChecker *cancellationChecker;
    };

    enum class ErrorCode {
        OK = 0,
        TIMEOUT = 1,
        CANCELLED = 2
    };

public:
    MemoryAgentAction(JNIEnv *env, jvmtiEnv *jvmti, jobject object);
    virtual ~MemoryAgentAction() = default;

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
    ProgressManager progressManager;
    JNIEnv *env;
    jvmtiEnv *jvmti;
};


#include "memory_agent_action.hpp"


#endif //MEMORY_AGENT_MEMORY_AGENT_ACTION_H
