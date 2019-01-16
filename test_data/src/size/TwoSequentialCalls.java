package size;

import common.TestBase;

public class TwoSequentialCalls extends TestBase {
  public static void main(String[] args) {
    Object object = new Object();
    printSize(object);
    printSize(object);
  }
}
