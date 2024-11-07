set(PROTO_FILES
    empty.proto
    device.proto
    radio.proto
)

add_library(kaonic_proto ${PROTO_FILES})

target_link_libraries(kaonic_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
)

target_include_directories(
    kaonic_proto

    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

protobuf_generate(TARGET kaonic_proto LANGUAGE cpp)

protobuf_generate(
    TARGET kaonic_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}"
)
