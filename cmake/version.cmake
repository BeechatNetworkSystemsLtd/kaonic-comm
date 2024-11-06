execute_process(
    COMMAND           "${GIT_EXECUTABLE}" describe --tags --abbrev=0
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE   git_tag_result
    OUTPUT_VARIABLE   git_tag
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT git_tag_result EQUAL 0)
    set(VERSION "v0.0.0")
else()
    set(VERSION ${git_tag})
endif()

if(VERSION MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    set(VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(VERSION_MINOR "${CMAKE_MATCH_2}")
    set(VERSION_PATCH "${CMAKE_MATCH_3}")
else()
    message(FATAL_ERROR "Git tag isn't a valid version of the form vX.X.X: ${VERSION}")
endif()

configure_file(${CMAKE_SOURCE_DIR}/kaonic/version.in ${CMAKE_BINARY_DIR}/version.hpp @ONLY)
include_directories(${CMAKE_BINARY_DIR})
