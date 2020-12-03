package com.intellij.memory.agent;


import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import java.io.File;

public class MemoryAgent {
    private final IdeaNativeAgentProxy proxy;

    private static class LazyHolder {
        static final MemoryAgent INSTANCE = new MemoryAgent();
        static Exception loadingException = null;
        static {
            try {
                File agentLib = AgentExtractor.extract(new File(System.getProperty("java.io.tmpdir")));
                System.load(agentLib.getAbsolutePath());
            } catch (Exception ex) {
                loadingException = ex;
            }
        }
    }

    private MemoryAgent() {
        proxy = new IdeaNativeAgentProxy();
    }

    public static MemoryAgent get() throws MemoryAgentException {
        Exception ex = LazyHolder.loadingException;
        if (ex != null) {
            throw new MemoryAgentException("Couldn't load memory agent", ex);
        }

        return LazyHolder.INSTANCE;
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
            throw new MemoryAgentException("Native method wasn't executed successfully");
        }

        return errorCodeAdResult[1];
    }
}
