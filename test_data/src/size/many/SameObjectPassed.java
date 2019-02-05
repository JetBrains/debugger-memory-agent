package size.many;

import common.Reference;
import common.TestBase;

public class SameObjectPassed extends TestBase {
  public static void main(String[] args) {
//    waitForDebugger();
    Object testObject = createTestObject();
    Reference ref = new Reference(testObject);
    testObject = null;

    printSizes(ref, ref);
  }
}
