package roots;

import common.Reference;
import common.TestBase;

public class StaticField extends TestBase {
  private static Object referent = createTestObject();
  private static Object staticField = new Reference(referent);

  public static void main(String[] args) {
    printGcRoots(referent);
  }
}
