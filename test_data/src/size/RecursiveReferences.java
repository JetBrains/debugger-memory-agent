package size;

import common.TestBase;

public class RecursiveReferences extends TestBase {
  public static void main(String[] args) {
    Object[] array1 = new Object[1];
    System.out.println("Empty array size");
    printSize(array1);
    Object[] array2 = new Object[]{array1};
    array1[0] = array2;
    System.out.println("With recursive link size");
    printSize(array1);
  }
}
