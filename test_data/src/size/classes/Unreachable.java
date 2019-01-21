package size.classes;

import common.TestBase;

public class Unreachable extends TestBase {
    public static void main(String[] args) {
        Unreachable instance = new Unreachable();
        System.out.println("Instance is reachable on the stack");
        printSizeByClasses(Unreachable.class);
        instance = null;
        System.out.println("Instance becomes unreachable");
        printSizeByClasses(Unreachable.class);
    }
}
