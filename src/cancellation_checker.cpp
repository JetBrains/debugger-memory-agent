// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include "cancellation_checker.h"
#include "utils.h"

bool CancellationChecker::shouldStopExecution() const {
    if (fileExists(cancellationFileName) || finishTime < std::chrono::steady_clock::now()) {
        return true;
    }
    return false;
}

bool CancellationChecker::shouldStopExecutionSyscallSafe() const {
    if (performedChecks == checksToPerform) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - lastSuccessfulCheck < std::chrono::seconds(1) &&
            checksToPerform < std::numeric_limits<unsigned int>::max() / 10) {
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
