package reachability;

import common.TestBase;
import common.TestTreeNode;

public class ClassReachability extends TestBase {
    public static void main(String[] args) {
        printClassReachability(Object.class);
        printClassReachability(TestTreeNode.class);
        printReachableObjects(TestTreeNode.class);
    }
}
