package roots;

import common.TestBase;

public class Field extends TestBase {
  private Object field;

  public static void main(String[] args) {
    Object testObject = createTestObject();
    Field obj = new Field();
    obj.field = testObject;
    printGcRoots(testObject);
  }

  @Override
  public String toString() {
    return "Field instance";
  }
}
