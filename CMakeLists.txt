cmake_minimum_required(VERSION 3.1)
project(memory_agent)

set(CMAKE_CXX_STANDARD 14)

if (WIN32 AND (NOT MINGW) AND (NOT CYGWIN))
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /O2")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/MT")
endif ()

find_package(JNI)
include_directories(${JNI_INCLUDE_DIRS})

add_library(memory_agent SHARED
        src/agent.cpp
        src/utils.cpp
        src/utils.h
        src/types.h
        src/gc_roots.h
        src/gc_roots.cpp
        src/objects_size.cpp
        src/objects_size.h
        src/size_by_classes.cpp
        src/size_by_classes.h
        src/log.cpp
        src/log.h)

if ((UNIX OR MINGW) AND NOT APPLE)
    target_link_libraries(memory_agent PRIVATE -static-libstdc++ -static-libgcc)
endif ()
