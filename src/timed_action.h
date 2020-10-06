// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_TIMED_ACTION_H
#define MEMORY_AGENT_TIMED_ACTION_H

#include <chrono>
#include <cstring>
#include "jni.h"
#include "jvmti.h"
#include "log.h"
#include "utils.h"

#define MEMORY_AGENT_TIMEOUT_ERROR static_cast<jvmtiError>(999)

struct CallbackWrapperData {
    CallbackWrapperData(const std::chrono::steady_clock::time_point &finishTime, void *callback, void *userData) :
            finishTime(finishTime), callback(callback), userData(userData) {

    }

    std::chrono::steady_clock::time_point finishTime;
    void *callback;
    void *userData;
};

template<typename RESULT_TYPE, typename... ARGS_TYPES>
class MemoryAgentTimedAction {
public:
    enum class ErrorCode {
        OK = 0,
        TIMEOUT = 1
    };

    MemoryAgentTimedAction(JNIEnv *env, jvmtiEnv *jvmti) : env(env), jvmti(jvmti) {

    }

    virtual ~MemoryAgentTimedAction() = default;

    jobjectArray runWithTimeout(jlong duration, ARGS_TYPES... args) {
        if (duration < 0) {
            finishTime = std::chrono::steady_clock::time_point::max();
        } else {
            finishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration);
        }

        RESULT_TYPE result = executeOperation(args...);
        jvmtiError err = cleanHeap();
        if (err != JVMTI_ERROR_NONE) {
            handleError(jvmti, err, "Couldn't clean heap");
        }

        jclass langObject = env->FindClass("java/lang/Object");
        jobjectArray returnValue = env->NewObjectArray(2, langObject, nullptr);
        ErrorCode errorCode = ErrorCode::OK;
        if (shouldStopExecution()) {
            errorCode = ErrorCode::TIMEOUT;
        }
        env->SetObjectArrayElement(returnValue, 0, toJavaArray(env, static_cast<jint>(errorCode)));
        env->SetObjectArrayElement(returnValue, 1, result);
        return returnValue;
    }

protected:
    virtual RESULT_TYPE executeOperation(ARGS_TYPES... args) = 0;
    virtual jvmtiError cleanHeap() = 0;

    bool shouldStopExecution() const { return finishTime < std::chrono::steady_clock::now(); }

    static jint JNICALL followReferencesCallbackWrapper(jvmtiHeapReferenceKind refKind, const jvmtiHeapReferenceInfo *refInfo, jlong classTag,
                                                        jlong referrerClassTag, jlong size, jlong *tagPtr,
                                                        jlong *referrerTagPtr, jint length, void *userData) {
        CallbackWrapperData *wrapperData = reinterpret_cast<CallbackWrapperData *>(userData);
        if (wrapperData->finishTime < std::chrono::steady_clock::now()) return JVMTI_VISIT_ABORT;
        return reinterpret_cast<jvmtiHeapReferenceCallback>(wrapperData->callback)(refKind, refInfo, classTag, referrerClassTag, size, tagPtr, referrerTagPtr, length, wrapperData->userData);
    }

    static jint JNICALL iterateThroughHeapCallbackWrapper(jlong classTag, jlong size, jlong *tagPtr, jint length, void *userData) {
        CallbackWrapperData *wrapperData = reinterpret_cast<CallbackWrapperData *>(userData);
        if (wrapperData->finishTime < std::chrono::steady_clock::now()) return JVMTI_VISIT_ABORT;
        return reinterpret_cast<jvmtiHeapIterationCallback>(wrapperData->callback)(classTag, size, tagPtr, length, wrapperData->userData);
    }

    jvmtiError FollowReferences(jint heapFilter, jclass klass, jobject initialObject,
                                jvmtiHeapReferenceCallback callback, void *userData,
                                const char *debugMessage=nullptr) {
        if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        if (debugMessage != nullptr) {
            debug(debugMessage);
        }

        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_reference_callback = followReferencesCallbackWrapper;

        CallbackWrapperData wrapperData(finishTime, reinterpret_cast<void *>(callback), userData);
        return jvmti->FollowReferences(heapFilter, klass, initialObject, &cb, &wrapperData);
    }

    jvmtiError IterateThroughHeap(jint heapFilter, jclass klass, jvmtiHeapIterationCallback callback,
                                  void *userData, const char *debugMessage=nullptr) {
        if (shouldStopExecution()) return MEMORY_AGENT_TIMEOUT_ERROR;

        if (debugMessage != nullptr) {
            debug(debugMessage);
        }

        jvmtiHeapCallbacks cb;
        std::memset(&cb, 0, sizeof(jvmtiHeapCallbacks));
        cb.heap_iteration_callback = iterateThroughHeapCallbackWrapper;

        CallbackWrapperData wrapperData(finishTime, reinterpret_cast<void *>(callback), userData);
        return jvmti->IterateThroughHeap(heapFilter, klass, &cb, &wrapperData);
    }

protected:
    JNIEnv *env;
    jvmtiEnv *jvmti;
    std::chrono::steady_clock::time_point finishTime;
};

#endif //MEMORY_AGENT_TIMED_ACTION_H
