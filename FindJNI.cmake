# This script rewrites the default FindJNI.cmake script to make
# the search process explicitly depend on the JDK_11 environmental variable
# instead of JAVA_HOME.

find_path(JAVA_INCLUDE_PATH jni.h
    HINTS $ENV{JDK_11}/include/
)

find_path(JAVA_AWT_INCLUDE_PATH jawt.h
    HINTS ${JAVA_INCLUDE_PATH}
)

find_path(JAVA_INCLUDE_PATH2 NAMES jni_md.h jniport.h
    HINTS
    ${JAVA_INCLUDE_PATH}
    ${JAVA_INCLUDE_PATH}/darwin
    ${JAVA_INCLUDE_PATH}/win32
    ${JAVA_INCLUDE_PATH}/linux
)

set(JNI_INCLUDE_DIRS
    ${JAVA_INCLUDE_PATH}
    ${JAVA_INCLUDE_PATH2}
    ${JAVA_AWT_INCLUDE_PATH}
)
