package com.intellij.memory.agent;


import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import java.io.File;

public class MemoryAgent {
    private static MemoryAgent INSTANCE = null;
    private final IdeaNativeAgentProxy proxy;

    private MemoryAgent() {
        proxy = new IdeaNativeAgentProxy();
    }

    public static synchronized MemoryAgent get() throws MemoryAgentException {
        if (INSTANCE != null) {
            return INSTANCE;
        }

        try {
            File agentLib = AgentExtractor.extract(new File(System.getProperty("java.io.tmpdir")));
            System.load(agentLib.getAbsolutePath());
            INSTANCE = new MemoryAgent();
            return INSTANCE;
        } catch (Exception ex) {
            throw new MemoryAgentException("Couldn't load memory agent", ex);
        }
    }

    @SuppressWarnings("unchecked")
    public synchronized <T> T getFirstReachableObject(Object startObject, Class<T> suspectClass) throws MemoryAgentException {
        return (T)getResult(proxy.getFirstReachableObject(startObject, suspectClass));
    }

    public synchronized Object[] getAllReachableObjects(Object startObject, Class<?> suspectClass) throws MemoryAgentException {
        return (Object[])getResult(proxy.getAllReachableObjects(startObject, suspectClass));
    }

    private static Object getResult(Object result) throws MemoryAgentException {
        Object[] errorCodeAdResult = (Object[])result;
        if (((int[])errorCodeAdResult[0])[0] != 0) {
            throw new MemoryAgentException("Failed to call native method");
        }

        return errorCodeAdResult[1];
    }
}
