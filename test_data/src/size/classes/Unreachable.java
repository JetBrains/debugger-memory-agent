package size.classes;

import common.TestBase;

public class Unreachable extends TestBase {
    public static void main(String[] args) {
        Unreachable instance = new Unreachable();
        System.out.println("Instance is reachable on the stack");
        printShallowSizeByClasses(Unreachable.class);
        instance = null;
        System.out.println("Instance becomes unreachable");
        printShallowSizeByClasses(Unreachable.class);
    }
}
