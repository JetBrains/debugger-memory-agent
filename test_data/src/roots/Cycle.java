package roots;

import common.Reference;
import common.TestBase;

public class Cycle extends TestBase {
  public static void main(String[] args) {
    Reference ref1 = new Reference(null);
    ref1.referent1 = new Reference(ref1);
    printGcRoots(ref1);
  }
}
