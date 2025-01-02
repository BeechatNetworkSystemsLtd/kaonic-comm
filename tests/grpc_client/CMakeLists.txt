add_executable(client)

target_sources(
    client

    PRIVATE
        src/main.cpp
)

target_link_libraries(
    client

    PRIVATE
        kaonic
)
