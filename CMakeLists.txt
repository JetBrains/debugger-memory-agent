cmake_minimum_required(VERSION 3.1)
project(memory_agent)

set(CMAKE_CXX_STANDARD 14)

if (WIN32 AND (NOT MINGW) AND (NOT CYGWIN))
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /O2")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/MT")
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(JNI11)
include_directories(${JNI_INCLUDE_DIRS})

add_library(memory_agent SHARED
        src/agent.cpp
        src/utils.cpp
        src/utils.h
        src/global_data.h
        src/log.cpp
        src/log.h
        src/memory_agent_action.h
        src/sizes/retained_size_and_held_objects.h
        src/sizes/retained_size_and_held_objects.cpp
        src/sizes/sizes_tags.h
        src/sizes/sizes_tags.cpp
        src/sizes/sizes_state.h
        src/sizes/sizes_state.cpp
        src/sizes/tag_info_array.h
        src/sizes/tag_info_array.cpp
        src/sizes/retained_size_by_classes.h
        src/sizes/retained_size_by_classes.cpp
        src/sizes/retained_size_action.h
        src/sizes/retained_size_by_objects.h
        src/sizes/retained_size_by_objects.cpp
        src/sizes/shallow_size_by_classes.cpp
        src/sizes/shallow_size_by_classes.h
        src/roots/infos.cpp src/roots/infos.h
        src/roots/roots_tags.cpp
        src/roots/roots_tags.h
        src/roots/roots_state.h
        src/roots/roots_state.cpp
        src/roots/paths_to_closest_gc_roots.cpp
        src/roots/paths_to_closest_gc_roots.h
        src/reachability/objects_of_class_in_heap.cpp
        src/reachability/objects_of_class_in_heap.h
        src/sizes/retained_size_action.cpp
        src/memory_agent_action.hpp
        src/cancellation_manager.cpp
        src/cancellation_manager.h
        src/allocation_sampling.cpp
        src/allocation_sampling.h
        src/progress_manager.cpp
        src/progress_manager.h
)

if ((UNIX OR MINGW) AND NOT APPLE)
    target_link_libraries(memory_agent PRIVATE -static-libstdc++ -static-libgcc)
    set_target_properties(memory_agent PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL")
endif ()
