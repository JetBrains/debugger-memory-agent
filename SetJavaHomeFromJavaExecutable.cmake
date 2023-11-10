if(NOT Java_JAVA_EXECUTABLE)
    message(FATAL_ERROR "java executable not found")
endif()
execute_process(COMMAND "${Java_JAVA_EXECUTABLE}" -XshowSettings:properties -version
        RESULT_VARIABLE res
        OUTPUT_VARIABLE var
        ERROR_VARIABLE var
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
if(res)
    message(FATAL_ERROR "Error executing java -version")
else()
    set(_java_version_regex "java\\.home = ([^\n]+)")
    if(var MATCHES "${_java_version_regex}")
        set(JAVA_HOME "${CMAKE_MATCH_1}")
    else()
        string(REPLACE "\n" "\n  " ver_msg "\n${var}")
        message(FATAL_ERROR "Java output not recognized:${ver_msg}\nPlease report.")
    endif()
endif()
