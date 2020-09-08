package size;

import common.TestBase;
import common.TestTreeNode;

public class WithHeldObjects5 extends TestBase {
    public static void main(String[] args) {
        /*
                  2 <--- 3
                /   \   /
               1 <--- 1
              / \
             1   1
        */
        TestTreeNode root2 = TestTreeNode.createTreeFromString("2 1 1 0 0 1 0 0 1 0 0");
        TestTreeNode root3 = new TestTreeNode.Impl3();
        root2.right.left = root2.left;
        printSizeAndHeldObjects(root2);
        root3.right = root2.right;
        root3.left = root2;
        printSizeAndHeldObjects(root2);
        root2 = null;
        printSizeAndHeldObjects(root3);
    }
}
