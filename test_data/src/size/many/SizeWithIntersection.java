package size.many;

import common.Reference;
import common.TestBase;

public class SizeWithIntersection extends TestBase {
  public static void main(String[] args) {
    Object testObject = createTestObject();
    Reference ref1 = new Reference(createTestObject(), testObject);
    printSizes(ref1);
    Reference ref2 = new Reference(testObject, createTestObject());
    printSizes(ref2);
    printSizes(ref1, ref2);
    testObject = null;
    printSizes(ref1, ref2);
  }
}
