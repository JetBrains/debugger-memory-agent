package com.intellij.memory.agent.proxy;

public class IdeaNativeAgentProxy {
  private IdeaNativeAgentProxy() {
  }

  public static native boolean canFindGcRoots();

  public static native boolean canEstimateObjectSize();

  public static native boolean canEstimateObjectsSizes();

  public static native long[] getShallowSizeByClasses(Class<?>[] klass);

  public static native Object gcRoots(Object object);

  public static native long size(Object object);

  public static boolean isLoaded() {
    try {
      return isLoadedImpl();
    } catch (Throwable t) {
      return false;
    }
  }

  private static native boolean isLoadedImpl();
}
