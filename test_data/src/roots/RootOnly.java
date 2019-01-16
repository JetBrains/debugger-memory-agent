package roots;

import common.TestBase;

public class RootOnly extends TestBase {
  public static void main(String[] args) {
    Object variable = createTestObject("only variable");
    printGcRoots(variable);
  }
}
