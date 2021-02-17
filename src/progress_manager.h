// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef PROGRESS_MANAGER_H
#define PROGRESS_MANAGER_H

#include <string>
#include <fstream>

/*
 * This class manages the progress of an operation and stores it in a json file.
 */
class ProgressManager {
protected:
    explicit ProgressManager(unsigned int maxValue = 100, unsigned int minValue = 0);
    virtual ~ProgressManager() = default;

    void updateProgress(unsigned int newValue, const std::string &message);

private:
    void updateProgressFileContents(const std::string &message);

protected:
    std::string progressFileName;

private:
    std::ofstream ofstream;
    unsigned int currentValue;
    unsigned int maxValue;
    unsigned int minValue;
};

#endif // PROGRESS_MANAGER_H
