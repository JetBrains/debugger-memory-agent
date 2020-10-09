// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_TIMED_ACTION_H
#define MEMORY_AGENT_TIMED_ACTION_H

#include <chrono>
#include <cstring>
#include "jni.h"
#include "jvmti.h"
#include "log.h"
#include "utils.h"

#define MEMORY_AGENT_INTERRUPTED_ERROR static_cast<jvmtiError>(999)

struct CallbackWrapperData {
    CallbackWrapperData(const std::chrono::steady_clock::time_point &finishTime,
                        void *callback, void *userData, const std::string &fileName);

    std::chrono::steady_clock::time_point finishTime;
    std::string cancellationFileName;
    void *callback;
    void *userData;
};

enum class ErrorCode {
    OK = 0,
    TIMEOUT = 1,
    CANCELLED = 2
};

ErrorCode getErrorCode(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName);

bool shouldStopExecution(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName);

// This function serves to minimize the number of syscalls during heap traversal by checking
// interruption only after visiting 'n' objects in heap
bool shouldStopExecutionDuringHeapTraversal(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName);

template<typename RESULT_TYPE, typename... ARGS_TYPES>
class MemoryAgentTimedAction {
public:
    MemoryAgentTimedAction(JNIEnv *env, jvmtiEnv *jvmti);
    virtual ~MemoryAgentTimedAction() = default;

    jobjectArray runWithTimeout(jlong duration, jstring interruptionFileName, ARGS_TYPES... args);
protected:
    virtual RESULT_TYPE executeOperation(ARGS_TYPES... args) = 0;
    virtual jvmtiError cleanHeap() = 0;

    bool shouldStopAction() const { return shouldStopExecution(finishTime, cancellationFileName); }

    static jint JNICALL followReferencesCallbackWrapper(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                                                        jlong *referrerTagPtr, jint length, void *userData);

    static jint JNICALL iterateThroughHeapCallbackWrapper(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData);

    jvmtiError FollowReferences(jint heapFilter, jclass klass, jobject initialObject,
                                jvmtiHeapReferenceCallback callback, void *userData,
                                const char *debugMessage=nullptr);

    jvmtiError IterateThroughHeap(jint heapFilter, jclass klass, jvmtiHeapIterationCallback callback,
                                  void *userData, const char *debugMessage=nullptr);

protected:
    JNIEnv *env;
    jvmtiEnv *jvmti;
    std::string cancellationFileName;
    std::chrono::steady_clock::time_point finishTime;
};


#include "timed_action.hpp"


#endif //MEMORY_AGENT_TIMED_ACTION_H
