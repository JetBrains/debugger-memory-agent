package roots;

import common.TestBase;
import common.TestNode;

public class ShortestPathsWhenObjectIsRoot extends TestBase {
    public static void main(String[] args) {
        TestNode root = new TestNode(1);
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root, 10, 1000));
    }
}
