package roots;

import common.TestBase;

public class WithPathsLimit extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    Object[] objects1 = new Object[]{testObject};
    Object[] objects2 = new Object[]{testObject};
    Object[] objects3 = new Object[]{testObject};
    Object[] objects4 = new Object[]{testObject};
    printGcRoots(testObject, 1, 1000);
    printGcRoots(testObject, 2, 1000);
    printGcRoots(testObject, 5, 1000);
  }
}
