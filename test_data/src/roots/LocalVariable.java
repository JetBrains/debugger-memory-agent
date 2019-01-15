package roots;

import common.Reference;
import common.TestBase;

public class LocalVariable extends TestBase {
  public static void main(String[] args) {
    Object target = createTestObject();
    Reference variable = new Reference(new Reference(target));
    printGcRoots(target);
  }
}
