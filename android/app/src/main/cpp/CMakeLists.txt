cmake_minimum_required(VERSION 3.22.1)
project("eathon_jni" C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Resolve repository root relative to android/app/src/main/cpp
get_filename_component(EATHON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

include_directories("${EATHON_ROOT}/compiler/include")

add_library(
    eathon_jni
    SHARED
    eathon_jni.c
    "${EATHON_ROOT}/compiler/src/eathonc.c"
    "${EATHON_ROOT}/compiler/src/eatdb.c"
)

target_compile_definitions(eathon_jni PRIVATE EATHON_LIB_MODE=1)

find_library(log-lib log)
if(log-lib)
    target_link_libraries(eathon_jni ${log-lib})
endif()
