#include "kaonic/comm/serial/hdlc.hpp"

namespace kaonic::comm::serial {

auto hdlc::escape(const hdlc_data_t& data, hdlc_data_t& output) noexcept -> void {
    output.clear();
    output.push_back(hdlc::flag);

    for (auto byte : data) {
        if (byte == hdlc::hdlc_esc) {
            output.push_back(hdlc::hdlc_esc);
            output.push_back(hdlc::hdlc_esc ^ hdlc::hdlc_esc_mask);
        } else if (byte == hdlc::flag) {
            output.push_back(hdlc::hdlc_esc);
            output.push_back(hdlc::flag ^ hdlc::hdlc_esc_mask);
        } else {
            output.push_back(byte);
        }
    }
    output.push_back(hdlc::flag);
}

auto hdlc::unescape(const hdlc_data_t& data, hdlc_data_t& output) noexcept -> void {
    auto in_frame = false;
    auto escape = false;

    output.clear();
    for (auto byte : data) {
        if (in_frame && byte == hdlc::flag) {
            in_frame = false;
            return;
        } else if (byte == hdlc::flag) {
            in_frame = true;
        } else if (in_frame) {
            if (byte == hdlc::hdlc_esc) {
                escape = true;
            } else {
                if (escape) {
                    if (byte == (hdlc::flag ^ hdlc::hdlc_esc_mask)) {
                        byte = hdlc::flag;
                    }
                    if (byte == (hdlc::hdlc_esc ^ hdlc::hdlc_esc_mask)) {
                        byte = hdlc::hdlc_esc;
                    }
                    escape = false;
                }
                output.push_back(byte);
            }
        }
    }
}

hdlc_processor::hdlc_processor(size_t max_hdlc_size) noexcept
    : _max_hdlc_size { max_hdlc_size } {}

auto hdlc_processor::update(uint8_t element, hdlc_data_t& output) noexcept -> bool {
    if (_in_frame && (output.size() + 1) > _max_hdlc_size) {
        _in_frame = false;
        output.clear();
        return false;
    }

    if (!_in_frame && element == hdlc::flag) {
        output.clear();
        _in_frame = true;
    }

    if (_in_frame && element == hdlc::flag) {
        output.push_back(hdlc::flag);
        _in_frame = false;
        return true;
    }

    if (_in_frame) {
        output.push_back(element);
    }

    return false;
}

} // namespace kaonic::comm::serial
