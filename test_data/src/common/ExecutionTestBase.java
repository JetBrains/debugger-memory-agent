package common;

import com.intellij.memory.agent.IdeaNativeAgentProxy;

import java.io.*;

public abstract class ExecutionTestBase extends TestBase {
    protected abstract TestBase.MemoryAgentErrorCode executeOperation(IdeaNativeAgentProxy proxy);

    public final void doProgressTest() throws IOException {
        IdeaNativeAgentProxy progressProxy = new IdeaNativeAgentProxy();
        progressProxy.progressFileName = System.getProperty("java.io.tmpdir") + "memory_agent_progress_file.json";
        executeOperation(progressProxy);
        try (BufferedReader br = new BufferedReader(new FileReader(progressProxy.progressFileName))) {
            String line;
            while ((line = br.readLine()) != null) {
                System.out.println(line);
            }
        }
    }

    public final void doTimeoutTest() throws IOException {
        long[] timeouts = new long[]{0, 1, DEFAULT_TIMEOUT};
        IdeaNativeAgentProxy timeoutProxy = new IdeaNativeAgentProxy();
        for (long timeout : timeouts) {
            timeoutProxy.timeoutInMillis = timeout;
            System.out.println(executeOperation(timeoutProxy));
        }

        File file = File.createTempFile(TestBase.DEFAULT_CANCELLATION_FILE_PATH, "");
        timeoutProxy.cancellationFileName = file.getPath();
        System.out.println(executeOperation(timeoutProxy));
    }

    public final void doTest() {
        try {
            doProgressTest();
            doTimeoutTest();
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }
}
