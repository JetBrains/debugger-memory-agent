package com.intellij.memory.agent;


import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;
import java.io.File;

public class MemoryAgent {
    private static final String ALLOCATION_SAMPLING_IS_NOT_SUPPORTED_MESSAGE = "Allocation sampling is not supported for this version of jdk";
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
    public <T> T getFirstReachableObject(Object startObject, Class<T> suspectClass) throws MemoryAgentException {
        return (T)getResult(proxy.getFirstReachableObject(startObject, suspectClass));
    }

    public Object[] getAllReachableObjects(Object startObject, Class<?> suspectClass) throws MemoryAgentException {
        return (Object[])getResult(proxy.getAllReachableObjects(startObject, suspectClass));
    }
    
    public void addAllocationListener(AllocationListener allocationListener) throws MemoryAgentException {
        if (!IdeaNativeAgentProxy.addAllocationListener(allocationListener)) {
            throw new MemoryAgentException(ALLOCATION_SAMPLING_IS_NOT_SUPPORTED_MESSAGE);
        }
    }

    public void setHeapSamplingInterval(long interval) throws MemoryAgentException {
        if (!IdeaNativeAgentProxy.setHeapSamplingInterval(interval)) {
            throw new MemoryAgentException(ALLOCATION_SAMPLING_IS_NOT_SUPPORTED_MESSAGE);
        }
    }

    public void clearListeners() {
        IdeaNativeAgentProxy.clearListeners();
    }

    private static Object getResult(Object result) throws MemoryAgentException {
        Object[] errorCodeAdResult = (Object[])result;
        if (((int[])errorCodeAdResult[0])[0] != 0) {
            throw new MemoryAgentException("Native method wasn't executed successfully");
        }

        return errorCodeAdResult[1];
    }
}
