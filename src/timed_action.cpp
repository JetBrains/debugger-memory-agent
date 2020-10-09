// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "timed_action.h"

#define INTERRUPTION_CHECK_INTERVAL 10000

CallbackWrapperData::CallbackWrapperData(const std::chrono::steady_clock::time_point &finishTime,
                                         void *callback, void *userData, const std::string &fileName) :
    finishTime(finishTime), callback(callback), userData(userData), cancellationFileName(fileName) {

}

ErrorCode getErrorCode(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName) {
    if (fileExists(cancellationFileName)) {
        return ErrorCode::CANCELLED;
    } else if (finishTime < std::chrono::steady_clock::now()) {
        return ErrorCode::TIMEOUT;
    }
    return ErrorCode::OK;
}

bool shouldStopExecution(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName) {
    return getErrorCode(finishTime, cancellationFileName) != ErrorCode::OK;
}

bool shouldStopExecutionDuringHeapTraversal(const std::chrono::steady_clock::time_point &finishTime, const std::string &cancellationFileName) {
    static int interval = 0;

    if (interval == INTERRUPTION_CHECK_INTERVAL) {
        interval = 0;
    }

    if (interval == 0) {
        interval++;
        return shouldStopExecution(finishTime, cancellationFileName);
    }

    return false;
}
