package roots;

import common.TestBase;

public class RootOnly extends TestBase {
  private static Object staticField = createTestObject("only static field");

  public static void main(String[] args) {
    Object variable = createTestObject("only variable");
    printGcRoots(variable);
    printGcRoots(staticField);
  }
}
