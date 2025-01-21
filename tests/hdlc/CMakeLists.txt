add_executable(hdlc)

target_sources(
    hdlc

    PRIVATE
        src/main.cpp
)

target_link_libraries(
    hdlc

    PRIVATE
        kaonic
)
