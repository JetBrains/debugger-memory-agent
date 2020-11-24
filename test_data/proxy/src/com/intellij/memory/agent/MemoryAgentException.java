package com.intellij.memory.agent;

public class MemoryAgentException extends Exception {
    public MemoryAgentException(String message) {
        super(message);
    }

    public MemoryAgentException(String message, Exception cause) {
        super(message, cause);
    }
}
