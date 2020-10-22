# Contributing guide for debugger memory agent
## Building
Once you've downloaded the project, you can build it using the following commands in the root project directory (Cmake 3.1 or higher required):
```
cmake .
make
```

Then make sure your `JAVA_HOME` environmental variable is set to JDK 1.8 or higher and run tests with:
```
python3 test_runner.py
```

## Development process
1. Pick an issue or figure out which ation you would like to implement. Every agent action is derived from `MemoryAgentTimedAction` 
and is implemented by overriding `cleanHeap` and `executeOperation` methods. Your action implementation must meet 3 requirements:

  * The action stops execution after interruption signal;
  * No tags are allocated after the execution;
  * All objects in the heap are tagged with 0 after the execution.

2. Write tests that cover use cases of your action and tests that cover action interruption. Then run all tests with `test_runner.py`. Note that if you didn't provide any test output it will be 
generated automatically on the first run. You should also provide correct output for JDK x86.
3. Submit a pull request and check the teamcity status. Then assign a reviewer if all tests are passing on three platforms.
