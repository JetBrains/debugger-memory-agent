package size;

import common.TestBase;

public class AfterReferenceShared extends TestBase {
  public static void main(String[] args) {
    Object obj = new Object();
    Object[] array = new Object[2];
    System.out.println("Empty array:");
    printSize(array);
    System.out.println("One link (referenced outside)");
    array[0] = obj;
    printSize(array);
    obj = null;
    System.out.println("One link (not referenced outside)");
    printSize(array);
    obj = array[0];
    array[1] = array[0];
    System.out.println("Two links (referenced outside)");
    printSize(array);
    obj = null;
    System.out.println("Two links (not referenced outside)");
    printSize(array);
  }
}
