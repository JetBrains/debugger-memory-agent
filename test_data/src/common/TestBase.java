package common;

import com.intellij.memory.agent.proxy.IdeaNativeAgentProxy;

import java.lang.management.ManagementFactory;
import java.util.*;
import java.util.stream.Collectors;

public abstract class TestBase {
  protected enum MemoryAgentErrorCode {
    OK(0),
    TIMEOUT(1);

    public final int id;

    public static MemoryAgentErrorCode valueOf(int value) {
      return value == 0 ? OK : TIMEOUT;
    }

    MemoryAgentErrorCode(int id) {
      this.id = id;
    }
  }

  static {
    if(IdeaNativeAgentProxy.isLoaded()) {
      System.out.println("Agent loaded");
    }
    else {
      System.out.println("Agent not loaded");
    }
  }

  private static final int DEFAULT_PATHS_LIMIT = 10;
  private static final int DEFAULT_OBJECTS_LIMIT = 5000;
  public static final long DEFAULT_TIMEOUT = -1;
  private static final Map<Integer, String> referenceDescription = new HashMap<>();

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
    referenceDescription.put(42, "TRUNCATE");
  }

  protected static long[] getResultAsLong(Object result) {
    return (long[])((Object[])result)[1];
  }

  protected static MemoryAgentErrorCode getErrorCode(Object result) {
    return MemoryAgentErrorCode.valueOf(((int[])((Object[])result)[0])[0]);
  }

  protected static void printClassReachability(Class<?> suspectClass) {
    Object result = IdeaNativeAgentProxy.getFirstReachableObject(null, suspectClass, TestBase.DEFAULT_TIMEOUT);
    System.out.println((((Object[]) result)[1]) != null);
  }

  protected static void printReachableObjects(Class<?> suspectClass) {
    Object result = IdeaNativeAgentProxy.getAllReachableObjects(null, suspectClass, TestBase.DEFAULT_TIMEOUT);
    Object[] objects  = (Object[]) ((Object[]) result)[1];
    printReachableObjects(objects, suspectClass);
  }

  protected static void printReachableObjects(Object startObject, Class<?> suspectClass) {
    Object result = IdeaNativeAgentProxy.getAllReachableObjects(startObject, suspectClass, TestBase.DEFAULT_TIMEOUT);
    Object[] objects  = (Object[]) ((Object[]) result)[1];
    printReachableObjects(objects, suspectClass);
  }

  private static void printReachableObjects(Object[] objects, Class<?> suspectClass) {
    System.out.println("Reachable objects of " + suspectClass + ":");
    printObjectsSortedByName(objects);
  }

  protected static void printSize(Object object) {
    Object result = IdeaNativeAgentProxy.size(object, DEFAULT_TIMEOUT);
    Object[] arrayResult = (Object[]) ((Object[]) result)[1];
    System.out.println(((long[])arrayResult[0])[1]);
  }

  protected static void printSizeAndHeldObjects(Object object) {
    Object result = IdeaNativeAgentProxy.size(object, DEFAULT_TIMEOUT);
    Object[] arrayResult = (Object[]) ((Object[]) result)[1];
    long[] sizes = ((long[]) arrayResult[0]);
    System.out.printf("Shallow size: %d, Retained size: %d\n", sizes[0], sizes[1]);
    System.out.println("Held objects:");
    printObjectsSortedByName((Object[])arrayResult[1]);
  }

  protected static void printObjectsSortedByName(Object[] objects) {
    List<String> objectsNames = new ArrayList<>();
    for (Object obj : objects) {
      objectsNames.add(obj.toString());
    }

    objectsNames.sort(String::compareTo);
    objectsNames.forEach(System.out::println);
  }

  protected static void printSizes(Object... objects) {
    String names = Arrays.toString(Arrays.stream(objects).map(TestBase::asString).toArray());
    System.out.println(names + " -> " + Arrays.toString(getResultAsLong(IdeaNativeAgentProxy.estimateRetainedSize(objects, DEFAULT_TIMEOUT))));
  }

  private static void printSizeByClasses(Class<?>[] classes, long[] sizes) {
    assertEquals(classes.length, sizes.length);
    for (int i = 0; i < sizes.length; i++) {
      System.out.println("\t" + classes[i].getTypeName() + " -> " + sizes[i]);
    }
  }

  protected static void printShallowAndRetainedSizeByClasses(Class<?>... classes) {
    assertTrue(IdeaNativeAgentProxy.canGetRetainedSizeByClasses());
    assertTrue(IdeaNativeAgentProxy.canGetShallowSizeByClasses());
    Object result = IdeaNativeAgentProxy.getShallowAndRetainedSizeByClasses(classes, DEFAULT_TIMEOUT);
    Object[] arrayResult = (Object[]) ((Object[]) result)[1];
    System.out.println("Shallow sizes by class:");
    printSizeByClasses(classes, (long[])arrayResult[0]);
    System.out.println("Retained sizes by class:");
    printSizeByClasses(classes, (long[])arrayResult[1]);
  }

  protected static void printSizeByClasses(Class<?>... classes) {
    assertTrue(IdeaNativeAgentProxy.canEstimateObjectsSizes());
    System.out.println("Shallow sizes by class:");
    printSizeByClasses(classes, getResultAsLong(IdeaNativeAgentProxy.getShallowSizeByClasses(classes, DEFAULT_TIMEOUT)));
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
    printGcRoots(object, DEFAULT_PATHS_LIMIT, DEFAULT_OBJECTS_LIMIT);
  }

  protected static void printGcRoots(Object object, int pathsLimit, int objectsLimit) {
    doPrintGcRoots(IdeaNativeAgentProxy.findPathsToClosestGcRoots(object, pathsLimit, objectsLimit, DEFAULT_TIMEOUT));
  }

  protected static void doPrintGcRoots(Object result) {
    Object[] arrayResult = (Object[]) ((Object[])result)[1];
    Object[] objects = (Object[]) arrayResult[0];
    Object[] links = (Object[]) arrayResult[1];
    boolean[] weakSoftReachable = (boolean[]) arrayResult[2];
    Object[] sortedObjects = Arrays.stream(objects).sorted(Comparator.comparing(TestBase::asString)).toArray();
    int[] oldIndexToNewIndex = new int[objects.length];
    int[] newIndexToOldIndex = new int[objects.length];
    for (int i = 0; i < sortedObjects.length; i++) {
      int old = indexOfReference(objects, sortedObjects[i]);
      oldIndexToNewIndex[old] = i;
      newIndexToOldIndex[i] = old;

    }
    for (int i = 0; i < sortedObjects.length; i++) {
      Object obj = sortedObjects[i];
      Object[] objLinks = (Object[]) links[newIndexToOldIndex[i]];
      int[] indices = (int[]) objLinks[0];
      int[] kinds = (int[]) objLinks[1];
      Object[] infos = (Object[]) objLinks[2];

      String referencedFrom = buildReferencesString(indices, kinds, infos, oldIndexToNewIndex);
      System.out.printf(
              "%d: %s %s<- %s\n",
              i, asString(obj),
              weakSoftReachable[newIndexToOldIndex[i]] ? "Weak/Soft reachable " : "",
              referencedFrom
      );
    }
  }

  private static String interpretInfo(int kind, Object info) {
    if (kind == 2 || kind == 8 // field or static field
        || kind == 3 // array element
        || kind == 9) { // constant pool element
      return "index = " + ((int[]) info)[0];
    } else if (kind == 42) { // other
      return ((int[]) info)[0] + " more";
    } else if (kind == 24 || kind == 25) { // stack local (jni)
      Object[] infos = (Object[]) info;
      assertEquals(2, infos.length);
      long[] stackInfo = (long[]) infos[0];
      return "thread id = " + stackInfo[0] + " depth = " + stackInfo[1] + " slot = " + stackInfo[2];
    }

    return "no details";
  }

  private static String buildReferencesString(int[] indices, int[] kinds, Object[] infos, int[] indicesMap) {
    assertEquals(indices.length, kinds.length);
    assertEquals(kinds.length, infos.length);

    List<String> references = new ArrayList<>();
    for (int i = 0; i < indices.length; i++) {
      int index = indices[i];
      String refFrom = index == -1 ? "root" : Integer.toString(indicesMap[index]);
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
      throw new AssertionError("Expected: " + expected.toString() + ", but actual: " + actual);
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
    return obj.toString().replaceFirst("@.*", "");
  }
}
