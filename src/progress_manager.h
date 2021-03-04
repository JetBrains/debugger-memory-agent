// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef PROGRESS_MANAGER_H
#define PROGRESS_MANAGER_H

#include <string>
#include <fstream>

/*
 * This class manages the progress of an operation and stores it in a json file.
 */
class ProgressManager {
public:
    explicit ProgressManager(unsigned int maxValue = 100, unsigned int minValue = 0);
    ~ProgressManager() = default;

    void updateProgress(unsigned int newValue, const std::string &message);
    void setProgressFileName(const std::string &newProgressFileName);

private:
    void updateProgressFileContents(const std::string &message);
    void writeProgressInfoInJSONFormat(const std::string &message, std::ofstream &ofstream) const;

private:
    std::string progressFileName;
    unsigned int currentValue;
    unsigned int maxValue;
    unsigned int minValue;
};

#endif // PROGRESS_MANAGER_H
