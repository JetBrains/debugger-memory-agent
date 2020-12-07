package com.intellij.memory.agent;

import java.io.*;
import java.util.Locale;

class AgentExtractor {
    private static File file = null;

    private AgentExtractor() {

    }

    static File extract(File directory) throws Exception {
        if (file != null) {
            return file;
        }

        AgentLibraryType agentType = detectAgentKind();
        if (agentType == null) {
            throw new Exception("Couldn't detect agent kind for current OS");
        }

        String agentFileName = agentType.prefix + "memory_agent";
        File f = new File(String.format("%s%c%s%s", directory.getAbsolutePath(), File.separatorChar, agentFileName, agentType.suffix));
        if (f.exists() && !f.isDirectory()) {
            file = f;
            return file;
        }

        file = File.createTempFile(agentFileName, agentType.suffix, directory);
        try(OutputStream outputStream = new FileOutputStream(file);
            InputStream inputStream = AgentExtractor.class.getClassLoader().getResourceAsStream( "bin/" + agentFileName + agentType.suffix)) {
            if (inputStream == null) {
                throw new MemoryAgentException("Agent binary library couldn't be found");
            }

            copyStream(inputStream, outputStream);
            return file;
        }
    }

    private static void copyStream(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    private static AgentLibraryType detectAgentKind() {
        String osName = System.getProperty("os.name").toLowerCase(Locale.ENGLISH);
        if (osName.startsWith("linux"))  {
            return AgentLibraryType.LINUX;
        } else if (osName.startsWith("mac")) {
            return AgentLibraryType.MACOS;
        } else if (osName.startsWith("windows")) {
            String bitness = System.getProperty("sun.arch.data.model");
            if (bitness.equals("32")) {
                return AgentLibraryType.WINDOWS32;
            } else {
                return AgentLibraryType.WINDOWS64;
            }
        }

        return null;
    }

    enum AgentLibraryType {
        WINDOWS32("", "32.dll"),
        WINDOWS64("", ".dll"),
        LINUX("lib", ".so"),
        MACOS("lib", ".dylib");

        AgentLibraryType(String prefix, String suffix) {
            this.prefix = prefix;
            this.suffix = suffix;
        }

        String prefix;
        String suffix;
    }
}
