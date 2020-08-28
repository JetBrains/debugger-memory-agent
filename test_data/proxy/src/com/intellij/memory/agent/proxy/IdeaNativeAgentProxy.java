package com.intellij.memory.agent.proxy;

public class IdeaNativeAgentProxy {
  private IdeaNativeAgentProxy() {
  }

  public static native boolean canEstimateObjectSize();

  public static native boolean canGetRetainedSizeByClasses();

  public static native boolean canGetShallowSizeByClasses();

  public static native boolean canEstimateObjectsSizes();

  public static native boolean canFindPathsToClosestGcRoots();

  public static native long[] getShallowSizeByClasses(Object[] classes);

  public static native long[] getRetainedSizeByClasses(Object[] classes);

  public static native Object getShallowAndRetainedSizeByClasses(Object[] classes);

  public static native Object findPathsToClosestGcRoots(Object object, int pathsNumber, int objectsNumber);

  public static native Object size(Object object);

  public static native long[] estimateRetainedSize(Object[] objects);

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private static native boolean isLoadedImpl();
}
