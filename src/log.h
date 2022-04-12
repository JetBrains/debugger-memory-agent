// Copyright 2000-2018 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license that can be found in the LICENSE file.

#ifndef MEMORY_AGENT_LOG_H
#define MEMORY_AGENT_LOG_H

namespace logger {
    void fatal(const char *msg);

    void error(const char *msg);

    void warn(const char *msg);

    void info(const char *msg);

    void debug(const char *msg);

    void resetTimer();

    void logPassedTime();

    void handleOptions(const char *options);

    void open(const char *fileName);

    void close();
}

#endif //MEMORY_AGENT_LOG_H
