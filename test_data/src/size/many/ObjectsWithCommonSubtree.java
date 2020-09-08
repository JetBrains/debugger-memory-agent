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
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 0 0 0");
        TestTreeNode root3 = TestTreeNode.createTreeFromString("3 0 1 0 0");
        root2.right = TestTreeNode.createTreeFromString("1 1 0 0 1 0 0");;
        root3.left = root2.right;
        printSizes(root2, root3, root2.right);
    }
}


