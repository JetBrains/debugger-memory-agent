package roots;

import common.TestBase;

public class WithLimit extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    Object[] objects = new Object[1000];
    for (int i = 0; i < objects.length; i++) {
      objects[i] = new Object[]{testObject};
    }

    printGcRoots(testObject, 1);
    printGcRoots(testObject, 2);
    printGcRoots(testObject, 5);
  }
}
