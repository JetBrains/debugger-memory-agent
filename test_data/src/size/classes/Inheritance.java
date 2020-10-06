package size.classes;

import common.TestBase;

public class Inheritance extends TestBase {
    static interface Interface {

    }

    static abstract class Abstract implements Interface {

    }

    static class First extends Abstract {

    }

    static class Second extends First {

    }

    static class Third extends Second {

    }

    public static void main(String[] args) {
        First first = new First();
        Second second = new Second();
        Third third = new Third();
        printShallowSizeByClasses(Interface.class, Abstract.class, First.class, Second.class, Third.class);
        printRetainedSizeByClasses(Interface.class, Abstract.class, First.class, Second.class, Third.class);
        printShallowAndRetainedSizeByClasses(Interface.class, Abstract.class, First.class, Second.class, Third.class);
    }
}
