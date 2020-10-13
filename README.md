[![official project](https://jb.gg/badges/official.svg)](https://confluence.jetbrains.com/display/ALL/JetBrains+on+GitHub)
[![GitHub license](https://img.shields.io/badge/license-Apache%20License%202.0-blue.svg?style=flat)](https://www.apache.org/licenses/LICENSE-2.0)

# Debugger memory agent
This is a native agent for JVM, which is currently used in the IDEA debugger to perform basic heap diagnostic during the debug process. To attach the agent run the JVM 
with the parameter: `-agentpath:/path/to/agent`

# Available features
After attaching this agent to JVM it can:
1. Calculate shallow and retained sizes by classes;
2. Calculate shallow and retained sizes by objects;
3. Find paths to closest gc roots from an object;
4. Calculate shallow and retained size of an object and get its retained set.
5. Find reachable objects of class in heap with handling weak/soft references.

# Contributing
See `CONTRIBUTING.md` for development tips before submitting a pull request. File an issue if you find any bug or have an idea for a new feature.
