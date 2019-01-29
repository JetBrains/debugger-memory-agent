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

  private static Map<Integer, String> referenceDescription = new HashMap<>();

  static {
    referenceDescription.put(1, "CLASS");
    referenceDescription.put(2, "FIELD");
    referenceDescription.put(3, "ARRAY_ELEMENT");
    referenceDescription.put(4, "CLASS_LOADER");
    referenceDescription.put(5, "SIGNERS");
    referenceDescription.put(6, "PROTECTION_DOMAIN");
    referenceDescription.put(7, "INTERFACE");
    referenceDescription.put(8, "STATIC_FIELD");
    referenceDescription.put(9, "CONSTANT_POOL");
    referenceDescription.put(10, "SUPERCLASS");
    referenceDescription.put(21, "JNI_GLOBAL");
    referenceDescription.put(22, "SYSTEM_CLASS");
    referenceDescription.put(23, "MONITOR");
    referenceDescription.put(24, "STACK_LOCAL");
    referenceDescription.put(25, "JNI_LOCAL");
    referenceDescription.put(26, "THREAD");
    referenceDescription.put(27, "OTHER");
  }

  protected static void printSize(Object object) {
    System.out.println(IdeaNativeAgentProxy.size(object));
  }

  protected static void printSizeByClasses(Class<?>... classes) {
    assertTrue(IdeaNativeAgentProxy.canEstimateObjectsSizes());
    long[] sizes = IdeaNativeAgentProxy.getShallowSizeByClasses(classes);
    assertEquals(classes.length, sizes.length);
    System.out.println("Shallow sizes by class:");
    for (int i = 0; i < sizes.length; i++) {
      System.out.println("\t" + classes[i].getTypeName() + " -> " + sizes[i]);
    }
  }

  private static int indexOfReference(Object[] array, Object value) {
    int result = -1;
    for (int i = 0; i < array.length; i++) {
      if (array[i] == value) {
        if (result != -1) {
          fail("Object " + value.toString() + " appeared twice in the objects list");
        }
        result = i;
      }
    }

    return result;
  }

  protected static void printGcRoots(Object object) {
    Object result = IdeaNativeAgentProxy.gcRoots(object);
    Object[] arrayResult = (Object[]) result;
    Object[] objects = (Object[]) arrayResult[0];
    Object[] links = (Object[]) arrayResult[1];
    Object[] sortedObjects = Arrays.stream(objects).sorted(Comparator.comparing(TestBase::asString)).toArray();
    Map<Integer, Integer> oldIndexToNewIndex = new HashMap<>();
    for (int i = 0; i < sortedObjects.length; i++) {
      oldIndexToNewIndex.put(indexOfReference(objects, sortedObjects[i]), i);
    }
    for (int i = 0; i < sortedObjects.length; i++) {
      Object obj = objects[i];
      Object[] objLinks = (Object[]) links[i];
      int[] indices = (int[]) objLinks[0];
      int[] kinds = (int[]) objLinks[1];
      Object[] infos = (Object[]) objLinks[2];

      String referencedFrom = buildReferencesString(indices, kinds, infos, oldIndexToNewIndex);
      System.out.println(i + ": " + asString(obj) + " <- " + referencedFrom);
    }
  }

  private static String interpretInfo(int kind, Object info) {
    if (kind == 2 || kind == 8 // field or static field
        || kind == 3 // array element
        || kind == 9) { // constant pool element
      return "index = " + ((int[]) info)[0];
    }

    if (kind == 24 || kind == 25) { // stack local (jni)
      Object[] infos = (Object[]) info;
      assertEquals(2, infos.length);
      long[] stackInfo = (long[]) infos[0];
      return "thread id = " + stackInfo[0] + " depth = " + stackInfo[1] + " slot = " + stackInfo[2];
    }

    return "no details";
  }

  private static String buildReferencesString(int[] indices, int[] kinds, Object[] infos, Map<Integer, Integer> indicesMap) {
    assertEquals(indices.length, kinds.length);
    assertEquals(kinds.length, infos.length);

    List<String> references = new ArrayList<>();
    for (int i = 0; i < indices.length; i++) {
      int index = indices[i];
      String refFrom = index == -1 ? "root" : indicesMap.get(index).toString();
      String kind = referenceDescription.get(kinds[i]);
      references.add(String.format("[%s :: %s :: %s]", refFrom, kind, interpretInfo(kinds[i], infos[i])));
    }

    references.sort(String::compareTo);

    return String.join(", ", references);
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

  protected static void fail(String message) {
    throw new AssertionError(message);
  }

  protected static void assertTrue(boolean condition, String message) {
    if (!condition) {
      throw new AssertionError(message);
    }
  }

  protected static void assertTrue(boolean condition) {
    assertTrue(condition, "Expected: true, but actual false");
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
