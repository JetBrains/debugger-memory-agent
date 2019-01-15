package roots;

import common.TestBase;

public class NestedArrays extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    Object[] objects = {new Object[]{testObject}};
    printGcRoots(testObject);
  }
}
