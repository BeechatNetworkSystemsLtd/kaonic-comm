#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kaonic::comm::serial {

using hdlc_data_t = std::vector<uint8_t>;

class hdlc final {

public:
    constexpr static uint8_t flag = 0x7E;
    constexpr static uint8_t hdlc_esc = 0x7D;
    constexpr static uint8_t hdlc_esc_mask = 0x20;

public:
    static auto escape(const hdlc_data_t& data, hdlc_data_t& output) noexcept -> void;

    static auto unescape(const hdlc_data_t& data, hdlc_data_t& output) noexcept -> void;

private:
};

class hdlc_processor final {

public:
    explicit hdlc_processor(size_t max_hdlc_size) noexcept;

    [[nodiscard]] auto update(uint8_t element, hdlc_data_t& output) noexcept -> bool;

private:
    size_t _max_hdlc_size;

    bool _in_frame = false;
};

} // namespace kaonic::comm::serial
