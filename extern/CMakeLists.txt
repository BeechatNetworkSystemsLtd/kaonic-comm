add_subdirectory(spdlog)
add_subdirectory(rf215)
add_subdirectory(rfnet)

target_compile_definitions(
    modules_rfnet

    PUBLIC
        -DRFNET_PORT_INCLUDE
)

target_include_directories(
    modules_rfnet

    PUBLIC
        ${CMAKE_SOURCE_DIR}/kaonic/include/kaonic/comm/mesh
)

