package roots;

import common.TestBase;

public class CallTwice extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    printGcRoots(testObject);
    printGcRoots(testObject);
  }
}
