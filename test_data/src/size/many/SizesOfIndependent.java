package size.many;

import common.Reference;
import common.TestBase;

public class SizesOfIndependent extends TestBase {
  public static void main(String[] args) {
    Reference ref1 = new Reference(createTestObject(), createTestObject());
    printSizes(ref1);
    Reference ref2 = new Reference(createTestObject());
    printSizes(ref2);

    printSizes(ref1, ref2);
  }
}
