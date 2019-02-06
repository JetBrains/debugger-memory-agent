package size.many;

import common.Reference;
import common.TestBase;

public class OneInSubtreeOfAnother extends TestBase {
  public static void main(String[] args) {
    Object testObject1 = createTestObject();
    Reference ref1 = new Reference(testObject1);
    printSizes(ref1);
    testObject1 = null;
    printSizes(ref1);

    Reference ref2 = new Reference(ref1, createTestObject());
    printSizes(ref1);
    printSizes(ref2);
    printSizes(ref1, ref2);
  }
}
