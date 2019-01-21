package size.classes;

import common.TestBase;

public class ManyClassesTwice extends TestBase {
    public static void main(String[] args) {
        ManyClasses.main(args);
        ManyClasses.main(args);
    }
}
