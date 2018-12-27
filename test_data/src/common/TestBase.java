package common;

import memory.agent.IdeaDebuggerNativeAgentClass;

import java.util.Objects;

public abstract class TestBase {
  static {
    if(IdeaDebuggerNativeAgentClass.isLoaded()) {
      System.out.println("Agent loaded");
    }
    else {
      System.out.println("Agent not loaded");
    }
  }

  protected static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected: true, but actual false");
    }
  }

  protected static void assertEquals(Object expected, Object actual) {
    if (expected == null) {
      if (actual != null) {
        throw new AssertionError("Expected: null, but actual: " + actual.toString());
      }
    } else if (!expected.equals(actual)) {
      throw new AssertionError("Expected: " + expected.toString() + ", but actual: " + Objects.toString(actual));
    }
  }
}
