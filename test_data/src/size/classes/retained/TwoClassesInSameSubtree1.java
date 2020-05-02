package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class TwoClassesInSameSubtree1 extends TestBase {
    public static void main(String[] args) {
        /*
                     1
                   /   \
                  2     3
                 / \
                3   1
               / \
              1   2
        */
        TestTreeNode root = TestTreeNode.createTreeFromString("1 2 3 1 0 0 2 0 0 1 0 0 3 0 0");
        printShallowAndRetainedSizeByClasses(TestTreeNode.Impl2.class, TestTreeNode.Impl3.class);
    }
}
