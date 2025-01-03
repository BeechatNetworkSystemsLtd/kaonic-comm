#pragma once

#include <string>

#include "kaonic/comm/protocols/hdlc.hpp"
#include "kaonic/common/error.hpp"

namespace kaonic::peripheral {

class peripheral {

public:
    virtual ~peripheral() = default;

    virtual auto read(protocols::hdlc_frame_t& frame) -> error = 0;
    virtual auto write(const protocols::hdlc_frame_t& frame) -> error = 0;

    virtual auto close() -> void = 0;

protected:
    explicit peripheral() noexcept = default;

    peripheral(const peripheral&) = default;
    peripheral(peripheral&&) = default;

    peripheral& operator=(const peripheral&) = default;
    peripheral& operator=(peripheral&&) = default;
};

} // namespace kaonic::peripheral
