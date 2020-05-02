package size.classes.retained;

import common.TestBase;
import common.TestTreeNode;

public class ThreeClassesInList extends TestBase {
    public static void main(String[] args) {
        /*
                        1
                       /
                      2
                     /
                    3
                   /
                  1
                 /
                2
               /
              3
        */
        TestTreeNode root = TestTreeNode.createTreeFromString("1 2 3 1 2 3 0 0 0 0 0 0 0");
        printShallowAndRetainedSizeByClasses(
                TestTreeNode.Impl1.class,
                TestTreeNode.Impl2.class,
                TestTreeNode.Impl3.class
        );
    }
}
