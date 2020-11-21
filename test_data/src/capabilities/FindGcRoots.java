package capabilities;

import common.TestBase;

public class FindGcRoots extends TestBase {
  public static void main(String[] args) {
    System.out.println(proxy.canFindPathsToClosestGcRoots());
  }
}
