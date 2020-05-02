package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesInSameSubtree2 extends TestBase {
    public static void main(String[] args) {
        /*
                        1
                      /   \
                     2     2
                   /   \
                  1     1
                 / \
                3   3
                   / \
                  1   1
        */
        TestTreeNode root = TestTreeNode.createTreeFromString("1 2 1 3 0 0 3 1 0 0 1 0 0 1 0 0 2 0 0");
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}
