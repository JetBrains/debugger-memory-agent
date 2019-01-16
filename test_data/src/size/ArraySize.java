package size;

import common.TestBase;

import java.util.Arrays;

public class ArraySize extends TestBase {
  public static void main(String[] args) {
    Object[] array = new Object[3];
    System.out.println(Arrays.toString(array));
    printSize(array);
    array[0] = new Integer(1);
    System.out.println(Arrays.toString(array));
    printSize(array);
    array[2] = new Integer(2);
    System.out.println(Arrays.toString(array));
    printSize(array);
    array[0] = null;
    System.out.println(Arrays.toString(array));
    printSize(array);
  }
}
