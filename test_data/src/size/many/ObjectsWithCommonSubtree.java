package size.many;

import common.TestBase;
import common.TestTreeNode;

public class ObjectsWithCommonSubtree extends TestBase {
    public static void main(String[] args) {
        /*
               2     3
             /   \ /   \
            1     1     1
                /   \
               1     1
        */
        TestTreeNode root1 = TestTreeNode.createTreeFromString("2 1 0 0 0");
        TestTreeNode root2 = TestTreeNode.createTreeFromString("3 0 1 0 0");
        root1.right = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");;
        root2.left = root1.right;
        printSizes(root1, root2, root1.right);
    }
}


