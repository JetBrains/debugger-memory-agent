[![JetBrains team project](http://jb.gg/badges/team.svg)](https://confluence.jetbrains.com/display/ALL/JetBrains+on+GitHub)
[![GitHub license](https://img.shields.io/badge/license-Apache%20License%202.0-blue.svg?style=flat)](https://www.apache.org/licenses/LICENSE-2.0)

# Memory agent
This is a native [JVMTI](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) agent for a JVM, which is currently used in the IDEA debugger to perform basic heap diagnostic during the debug process. It can also be used in any project, that is running on a JVM, to attach allocation sampling listeners for [low-overhead heap profiling](https://openjdk.java.net/jeps/331) and to collect information about the heap, such as retained sizes, object reachability, and paths to closest GC roots.

# Example usage
## Allocation sampling
Using the memory agent, you can attach allocation listeners that will catch allocation sampling events and execute your code. In the example below a stack trace of each allocation is printed.
```
MemoryAgent agent = MemoryAgent.get();
agent.addAllocationListener((info) -> {
   for (StackTraceElement element : info.getThread().getStackTrace()) {
       System.out.println(element);
   }
});
```
You can also change heap sampling interval with
```
agent.setHeapSamplingInterval(512);
```

## Checking for object reachability
The example below shows how to check object reachability in the heap with weak/soft/phantom reference handling.
```
MemoryAgent agent = MemoryAgent.get();
// Passing null here forces the agent to traverse heap from the GC roots
System.out.println(agent.getFirstReachableObject(null, TestClass.class));
for (TestClass instance : agent.getAllReachableObjects(null, TestClass.class)) {
    System.out.println(instance);
}
```

# Using the agent in your projects
The library is published to the bintray [repository](https://bintray.com/jetbrains/intellij-third-party-dependencies/debugger-memory-agent).

## Maven
Add the external repository url:
```
<repositories>
  <repository>
    <id>jetbrains.bintray</id>
    <url>https://jetbrains.bintray.com/intellij-third-party-dependencies</url>
  </repository>
</repositories>
```
Then add the dependency:
```
<dependencies>
  <dependency>
    <groupId>org.jetbrains.intellij.deps</groupId>
    <artifactId>debugger-memory-agent</artifactId>
    <version>1.0.32</version>
  </dependency>
</dependencies>
```
##  Gradle
Add the external repository url:
```
repositories {
  maven {
    url "https://jetbrains.bintray.com/intellij-third-party-dependencies"
  }
}
```
Then add the dependency:
```
dependencies {
  implementation 'org.jetbrains.intellij.deps:debugger-memory-agent:1.0.32'
}
```

## Gradle Kotlin DSL
Add the external repository url:
```
repositories {
  maven {
    url = uri("https://jetbrains.bintray.com/intellij-third-party-dependencies")
  }
}
```
Then add the dependency:
```
dependencies {
  implementation("org.jetbrains.intellij.deps:debugger-memory-agent:1.0.32")
}
```

## Attaching the agent to a JVM
If the agent library doesn't meet your needs, you can still attach the agent to a JVM separately. To do so, you should compile the agent library following the steps in [CONTRIBUTING.md](CONTRIBUTING.md). Then run a JVM with the parameter: 

`-agentpath:/path/to/agent/library`. 

After that, you can call the memory agent's methods using the [IdeaNativeAgentProxy](test_data/proxy/src/com/intellij/memory/agent/IdeaNativeAgentProxy.java) class.

# Building and contributing
See [CONTRIBUTING.md](CONTRIBUTING.md) for building and development tips before submitting a pull request. File an issue if you find any bug or have an idea for a new feature.
