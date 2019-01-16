package size;

import common.TestBase;

public class PrimitiveArraySize extends TestBase {
  public static void main(String[] args) {
    int[] array = new int[5];
    printSize(array);

    array = new int[6];
    printSize(array);

    array[5] = new Integer(5000);
    printSize(array);
  }
}
