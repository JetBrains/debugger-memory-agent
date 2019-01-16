package common;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;

import java.lang.management.ManagementFactory;
import java.util.*;
import java.util.stream.Collectors;

public abstract class TestBase {
  static {
    if(IdeaNativeAgentProxy.isLoaded()) {
      System.out.println("Agent loaded");
    }
    else {
      System.out.println("Agent not loaded");
    }
  }

  protected static void printSize(Object object) {
    System.out.println(IdeaNativeAgentProxy.size(object));
  }

  protected static void printGcRoots(Object object) {
    Object result = IdeaNativeAgentProxy.gcRoots(object);
    Object[] arrayResult = (Object[]) result;
    Object[] objects = (Object[]) arrayResult[0];
    Object[] links = (Object[]) arrayResult[1];
    List<String> lines = new ArrayList<>();
    for (int i = 0; i < objects.length; i++) {
      Object obj = objects[i];
      int[] objLinks = (int[]) links[i];
      String referencesFrom = Arrays.stream(objLinks).mapToObj(x -> asString(objects[x])).collect(Collectors.joining(", "));
      lines.add(asString(obj) + " <- " + referencesFrom);
    }

    lines.sort(String::compareTo);
    lines.forEach(System.out::println);
  }

  protected static Object createTestObject(String name) {
    return new Object() {
      @Override
      public String toString() {
        return name;
      }
    };
  }

  protected static Object createTestObject() {
    return createTestObject("test object");
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

  protected static void waitForDebugger() {
    System.out.println(ManagementFactory.getRuntimeMXBean().getName());
    try {
      Thread.sleep(20000);
    } catch (InterruptedException ignored) {
    }
  }

  private static String asString(Object obj) {
    return asStringImpl(obj, new HashMap<>());
  }

  private static String asStringImpl(Object obj, Map<Object, Integer> visited) {
    if (visited.containsKey(obj)) {
      return "#recursive" + visited.get(obj) + "#";
    }

    visited.put(obj, visited.size());
    if (obj == null) return "null";
    if (obj instanceof Object[]) {
      return Arrays.stream((Object[]) obj).map(x -> asStringImpl(x, visited)).collect(Collectors.joining(", ", "[", "]"));
    }
    return obj.toString();
  }
}
