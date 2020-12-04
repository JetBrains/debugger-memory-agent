package roots;

import common.TestBase;
import common.TestNode;

public class ShortestPathsForUnreachable extends TestBase {
    public static void main(String[] args) {
        doPrintGcRoots(proxy.findPathsToClosestGcRoots(new TestNode(3), 10, 1000));
    }
}
