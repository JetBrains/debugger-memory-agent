package roots;

import common.TestBase;
import common.TestNode;

public class WithObjectsLimit extends TestBase {
    public static void main(String[] args) {
        TestNode root1 = new TestNode(10);
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root1.getLast(), 5, 10));
        TestNode root2 = new TestNode(5000, root1.getLast());
        TestNode root3 = new TestNode(1000, root1.getLast());
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root1.getLast(), 5, 10));
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(root1.getLast(), 5, 20));
    }
}
