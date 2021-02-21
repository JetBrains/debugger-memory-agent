# This script rewrites the default FindJNI.cmake script to make
# the search process explicitly depend on the JDK_11 environmental variable
# instead of JAVA_HOME.

set(JAVA_INCLUDE_PATH $ENV{JDK_11}/include/)

if (WIN32)
    set(JAVA_INCLUDE_PATH2 ${JAVA_INCLUDE_PATH}win32)
elseif(UNIX AND NOT APPLE)
    set(JAVA_INCLUDE_PATH2 ${JAVA_INCLUDE_PATH}linux)
elseif(APPLE)
    set(JAVA_INCLUDE_PATH2 ${JAVA_INCLUDE_PATH}darwin)
endif()

set(JNI_INCLUDE_DIRS
    ${JAVA_INCLUDE_PATH}
    ${JAVA_INCLUDE_PATH2}
)
