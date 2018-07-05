cmake_minimum_required(VERSION 3.10)
project(native_memory_agent)

set(CMAKE_CXX_STANDARD 14)
include_directories("C:\\Program Files\\Java\\jdk1.8.0_121\\include\\win32"
        "C:\\Program Files\\Java\\jdk1.8.0_121\\include")
add_library(native_memory_agent SHARED
        src/agent.cpp
        src/utils.cpp
        src/utils.h
        src/types.h
        src/gc_roots.h
        src/gc_roots.cpp
        src/object_size.h
        src/obect_size.cpp)