package com.intellij.memory.agent;

public class MemoryAgentInitializationException extends Exception {
    public MemoryAgentInitializationException(String message) {
        super(message);
    }

    public MemoryAgentInitializationException(String message, Exception cause) {
        super(message, cause);
    }
}
