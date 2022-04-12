// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#include <string>
#include <iostream>
#include <fstream>
#include "log.h"

namespace logger {
    enum LogLevel {
        NO_LOG = 0,
        FATAL = 1,
        ERROR = 2,
        WARN = 3,
        INFO = 4,
        DEBUG = 5
    };

    LogLevel LEVEL = ERROR;
    std::chrono::steady_clock::time_point timePoint;
    std::ostream *out = &std::cout;

    void open(const char *fileName) {
        out = new std::ofstream(fileName);
    }

    void close() {
        if (out != &std::cout) {
            delete out;
        }
    }

    void fatal(const char *msg) {
        if (LEVEL >= FATAL) {
            *out << "MEMORY_AGENT::FATAL " << msg << std::endl;
        }
    }

    void error(const char *msg) {
        if (LEVEL >= ERROR) {
            *out << "MEMORY_AGENT::ERROR " << msg << std::endl;
        }
    }

    void warn(const char *msg) {
        if (LEVEL >= WARN) {
            *out << "MEMORY_AGENT::WARN " << msg << std::endl;
        }
    }

    void info(const char *msg) {
        if (LEVEL >= INFO) {
            *out  << "MEMORY_AGENT::INFO " << msg << std::endl;
        }
    }

    void debug(const char *msg) {
        if (LEVEL >= DEBUG) {
            *out << "MEMORY_AGENT::DEBUG " << msg << std::endl;
        }
    }

    void resetTimer() {
        timePoint = std::chrono::steady_clock::now();
    }

    void logPassedTime() {
        if (LEVEL >= DEBUG) {
            long long passedTime = std::__1::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - timePoint
            ).count();
            *out << "MEMORY_AGENT::TIME_MS " << passedTime << std::endl;
        }
    }

    void handleOptions(const char *options) {
        auto level = static_cast<int>(LEVEL);
        if (options && !std::string(options).empty()) {
            try {
                level = std::stoi(options);
            }
            catch (const std::invalid_argument &e) {
                logger::error("Invalid log level argument. Expected value is integer between 0 and 5. \"ERROR\" level will be set");
            }
        }

        if (NO_LOG <= level && level <= DEBUG) {
            LEVEL = static_cast<LogLevel>(level);
        } else {
            logger::warn("Unexpected log level. Expected value between 0 and 5");
        }
    }
}
