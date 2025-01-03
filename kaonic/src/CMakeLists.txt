add_library(kaonic)

target_include_directories(
    kaonic

    PUBLIC
        ../include
)

target_sources(
    kaonic

    PRIVATE
        comm/radio/rf215_radio.cpp

        comm/peripheral/peripheral_dispatcher.cpp
        comm/peripheral/usb_peripheral.cpp

        grpc/context.cpp
        grpc/device_service.cpp
        grpc/radio_service.cpp
)

target_link_libraries(
    kaonic

    PUBLIC
        spdlog::spdlog
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
