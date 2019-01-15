package common;

@SuppressWarnings({"FieldCanBeLocal", "unused"})
public class Reference {
  private Object referent1;
  private Object referent2;
  private static int num = 0;
  private final int number;

  public Reference(Object referent1, Object referent2) {
    this.referent1 = referent1;
    this.referent2 = referent2;
    number = ++num;
  }

  public Reference(Object referent) {
    this(referent, null);
  }

  @Override
  public String toString() {
    return "[ref " + number + "]";
  }
}
