package roots;

import common.Reference;
import common.TestBase;

public class Unreachable extends TestBase {
  public static void main(String[] args) {
    printGcRoots(new Reference(createTestObject()).referent1);
  }
}
