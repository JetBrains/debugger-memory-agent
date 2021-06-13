package com.intellij.memory.agent;

public class IdeaNativeAgentProxy {
  public String cancellationFileName;
  public String progressFileName;
  public long timeoutInMillis;

  static {
    String agentPath = System.getProperty("intellij.memory.agent.path");
    if (agentPath != null) {
      System.load(agentPath);
    }
  }

  public IdeaNativeAgentProxy() {
    cancellationFileName = "";
    progressFileName = "";
    timeoutInMillis = -1;
  }

  public IdeaNativeAgentProxy(Object cancellationFileName, Object progressFileName, long timeoutInMillis) {
    this.cancellationFileName = (String)cancellationFileName;
    this.progressFileName = (String)progressFileName;
    this.timeoutInMillis = timeoutInMillis;
  }

  public native boolean canEstimateObjectSize();

  public native boolean canGetRetainedSizeByClasses();

  public native boolean canGetShallowSizeByClasses();

  public native boolean canEstimateObjectsSizes();

  public native boolean canFindPathsToClosestGcRoots();

  public native Object[] getShallowSizeByClasses(Object[] classes);

  public native Object[] getRetainedSizeByClasses(Object[] classes);

  public native Object[] getShallowAndRetainedSizeByClasses(Object[] classes);

  public native Object[] findPathsToClosestGcRoots(Object object, int pathsNumber, int objectsNumber);

  public native Object[] size(Object object);

  public native Object[] estimateRetainedSize(Object[] objects);

  public native Object[] getFirstReachableObject(Object startObject, Object suspectClass);

  public native Object[] getAllReachableObjects(Object startObject, Object suspectClass);

  static native boolean setHeapSamplingInterval(long interval);

  static native boolean initArrayOfListeners(Object array);

  static native boolean enableAllocationSampling();

  static native boolean disableAllocationSampling();

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private static native boolean isLoadedImpl();
}
