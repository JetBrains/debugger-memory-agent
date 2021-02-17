package com.intellij.memory.agent;

public class MemoryAgentExecutionException extends Exception {
    public MemoryAgentExecutionException(String message) {
        super(message);
    }

    public MemoryAgentExecutionException(String message, Exception cause) {
        super(message, cause);
    }
}
