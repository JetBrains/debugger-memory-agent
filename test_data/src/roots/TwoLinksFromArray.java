package roots;

import common.TestBase;

public class TwoLinksFromArray extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    Object[] objs = new Object[]{testObject, null, testObject};
    printGcRoots(testObject);
  }
}
