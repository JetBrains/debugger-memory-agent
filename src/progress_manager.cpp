
#include "progress_manager.h"

ProgressManager::ProgressManager(unsigned int maxValue, unsigned int minValue) :
    maxValue(maxValue), minValue(minValue), currentValue(minValue) {

}

void ProgressManager::updateProgress(unsigned int newValue, const std::string &message) {
    if (newValue < currentValue) {
        return;
    } else if (newValue < minValue) {
        newValue = minValue;
    } else if (newValue > maxValue) {
        newValue = maxValue;
    }
    currentValue = newValue;

    if (!progressFileName.empty()) {
        updateProgressFileContents(message);
    }
}

void ProgressManager::writeProgressInfoInJSONFormat(const std::string &message, std::ofstream &ofstream) const {
    const char *ending = ",\n\t";
    ofstream << "{\n\t";
    ofstream << "\"minValue\": " << minValue << ending;
    ofstream << "\"maxValue\": " << maxValue << ending;
    ofstream << "\"currentValue\": " << currentValue << ending;
    ofstream << "\"message\": " << "\"" << message << "\"\n";
    ofstream << "}";
}

void ProgressManager::updateProgressFileContents(const std::string &message) {
    std::ofstream ofstream(progressFileName);
    if (ofstream) {
        writeProgressInfoInJSONFormat(message, ofstream);
        ofstream.close();
    }
}

void ProgressManager::setProgressFileName(const std::string &newProgressFileName) {
    progressFileName = newProgressFileName;
}
