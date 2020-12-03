// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_CANCELLATION_MANAGER_H
#define MEMORY_AGENT_CANCELLATION_MANAGER_H

#include <chrono>
#include <string>

class CancellationManager {
public:
    CancellationManager(const std::string &cancellationFileName, const std::chrono::steady_clock::time_point &finishTime);
    CancellationManager() = default;

    ~CancellationManager() = default;

    /*
     * NOTE: This method is time consuming because it uses syscalls to check cancellation
     * @returns true if class user should stop execution, otherwise false
     */
    bool shouldStopExecution() const;

    /*
     * NOTE: This method is intended to minimize the number of syscalls
     * when frequent cancellation checking is necessary (e.g. during heap traversals)
     * @returns true once in a number of method calls if class user should stop execution, otherwise false
     */
    bool shouldStopExecutionSyscallSafe() const;

protected:
    std::string cancellationFileName;
    std::chrono::steady_clock::time_point finishTime;

private:
    const static unsigned int defaultChecksToPerformValue = 10000;
    mutable std::chrono::steady_clock::time_point lastSuccessfulCheck;
    mutable unsigned int checksToPerform;
    mutable unsigned int performedChecks;
};


#endif //MEMORY_AGENT_CANCELLATION_MANAGER_H
