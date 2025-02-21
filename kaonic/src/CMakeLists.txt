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

        comm/drivers/spi.cpp

        comm/serial/serial.cpp
        comm/serial/hdlc.cpp
        comm/serial/packet.cpp

        comm/mesh/network.cpp
        comm/mesh/radio_network.cpp
        comm/mesh/network_receiver.cpp

        comm/services/radio_service.cpp
        comm/services/grpc_service.cpp
        comm/services/serial_service.cpp
)

target_link_libraries(
    kaonic

    PRIVATE
        -lgpiodcxx
        -lz

    PUBLIC
        spdlog::spdlog
        kaonic_proto
        drivers_rf215
        modules_rfnet
)

add_executable(kaonic_commd)

target_sources(
    kaonic_commd

    PRIVATE
        main.cpp
)

target_link_libraries(
    kaonic_commd

    PRIVATE
        kaonic
)

set_target_properties(kaonic_commd PROPERTIES OUTPUT_NAME "kaonic-commd")

install(TARGETS kaonic_commd DESTINATION bin)

