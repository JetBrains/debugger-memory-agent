package common;

public abstract class TimeoutTestBase extends TestBase {
    protected abstract TestBase.MemoryAgentErrorCode executeOperation(long timeoutInMillis);

    public final void doTest() {
        long[] timeouts = new long[]{0, 1, DEFAULT_TIMEOUT};
        for (long t : timeouts) {
            System.out.println(executeOperation(t));
        }
    }
}
