cmake_minimum_required(VERSION 3.1)

# always build universal binaries on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

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
        src/log.cpp
        src/sizes/retained_size_and_held_objects.cpp
        src/sizes/sizes_tags.cpp
        src/sizes/sizes_state.cpp
        src/sizes/tag_info_array.cpp
        src/sizes/retained_size_by_classes.cpp
        src/sizes/retained_size_by_objects.cpp
        src/sizes/shallow_size_by_classes.cpp
        src/roots/roots_tags.cpp
        src/roots/roots_state.cpp
        src/roots/paths_to_closest_gc_roots.cpp
        src/roots/infos.cpp
        src/reachability/objects_of_class_in_heap.cpp
        src/sizes/retained_size_action.cpp
        src/cancellation_checker.cpp
        src/allocation_sampling.cpp
        src/progress_manager.cpp
        src/sizes/retained_size_via_dominator_tree.cpp
        src/sizes/dominator_tree.cpp
)

if ((UNIX OR MINGW) AND NOT APPLE)
    target_link_libraries(memory_agent PRIVATE -static-libstdc++ -static-libgcc)
    set_target_properties(memory_agent PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL")
endif ()

if(WIN32)
    include(CheckSymbolExists)
    check_symbol_exists("_M_ARM64" "" TARGET_ARM64)
    check_symbol_exists("_M_AMD64" "" TARGET_X64)
    if(TARGET_ARM64)
        set(FILE_NAME "memory_agent64a")
    elseif(TARGET_X64)
        set(FILE_NAME "memory_agent") # Consider using '64' suffix
    else()
        set(FILE_NAME "memory_agent32")
    endif()
    set_target_properties(memory_agent PROPERTIES OUTPUT_NAME ${FILE_NAME})
endif()
