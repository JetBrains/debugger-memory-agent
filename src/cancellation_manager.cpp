// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "cancellation_manager.h"
#include "utils.h"

CancellationManager::CancellationManager(const std::string &cancellationFileName, const std::chrono::steady_clock::time_point &finishTime) :
 finishTime(finishTime), cancellationFileName(cancellationFileName), checksToPerform(defaultChecksToPerformValue), performedChecks(0) {

}

bool CancellationManager::shouldStopExecution() const {
    if (fileExists(cancellationFileName) || finishTime < std::chrono::steady_clock::now()) {
        return true;
    }
    return false;
}

bool CancellationManager::shouldStopExecutionSyscallSafe() const {
    if (performedChecks == checksToPerform) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - lastSuccessfulCheck < std::chrono::seconds(1)) {
            checksToPerform *= 10;
        } else {
            checksToPerform = defaultChecksToPerformValue;
        }
        lastSuccessfulCheck = now;
        performedChecks = 0;
        return fileExists(cancellationFileName) || finishTime < now;
    }

    performedChecks++;
    return false;
}
