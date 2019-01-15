package roots;

import common.TestBase;

public class Array extends TestBase {
  public static void main(String[] args) throws InterruptedException {
    Object target = createTestObject();
    Object[] array = new Object[]{target, null};
    printGcRoots(target);
  }
}
