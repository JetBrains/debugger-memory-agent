package size.classes;

import common.TestBase;

public class ManyClasses extends TestBase {
    public static void main(String[] args) {
        class SecondClass {
        }
        class ThirdClass {
        }

        SecondClass secondClassObject = new SecondClass();
        SecondClass oneMoreInstance = new SecondClass();
        size.classes.OneClass oneClass = new size.classes.OneClass();
        printSizeByClasses(ThirdClass.class, OneClass.class, SecondClass.class);
    }
}
