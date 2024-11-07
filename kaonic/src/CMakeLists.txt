add_library(kaonic)

target_include_directories(
    kaonic

    PUBLIC
        ../include
)

target_sources(
    kaonic

    PRIVATE
        grpc/device_service.cpp
        grpc/radio_service.cpp
)

target_link_libraries(
    kaonic

    PUBLIC
        kaonic_proto
)

add_executable(server)

target_sources(
    server

    PRIVATE
        main.cpp
)

target_link_libraries(
    server

    PRIVATE
        kaonic
)

install(TARGETS server DESTINATION bin)
