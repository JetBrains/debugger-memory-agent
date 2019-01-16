package roots;

import common.Reference;
import common.TestBase;

public class TwoLinksFromOneObject extends TestBase {
  public static void main(String[] args) {
    Object o = createTestObject();
    Reference reference = new Reference(o, o);
    o = null;
    printGcRoots(reference.referent2);
  }
}
