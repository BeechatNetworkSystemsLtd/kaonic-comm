#include <numeric>
#include <sstream>

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/comm/serial/hdlc.hpp"
#include "kaonic/comm/serial/packet.hpp"
#include "kaonic/common/logging.hpp"

using namespace kaonic;

static auto buf_pack(const RadioFrame& src, std::vector<uint8_t>& dst) -> void {
    const auto& data = src.data();
    dst.resize(data.size() * sizeof(uint32_t));
    memcpy(dst.data(), data.data(), data.size() * sizeof(uint32_t));
    dst.resize(src.length());
}

static auto buf_unpack(const std::vector<uint8_t>& src, RadioFrame& dst) -> void {
    auto data = dst.mutable_data();

    size_t dst_size = src.size() / sizeof(uint32_t);
    dst_size += (src.size() - dst_size * sizeof(uint32_t)) ? 1 : 0;
    data->Resize(dst_size, 0);
    memcpy(data->begin(), src.data(), src.size());

    dst.set_length(src.size());
}

static auto vector_to_string(const std::vector<uint8_t>& vec) noexcept -> std::string {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
            oss << " ";
        oss << static_cast<uint16_t>(vec[i]);
    }
    return oss.str();
}

static auto test_radio_to_client() -> int {
    log::info("[HDLC Test] Radio to Client test");

    comm::mesh::frame rx_test_frame;
    rx_test_frame.buffer.resize(10);

    std::iota(rx_test_frame.buffer.begin(), rx_test_frame.buffer.end(), 1);

    log::info("[HDLC Test] Input frame elements:");
    log::info(vector_to_string(rx_test_frame.buffer));

    comm::serial::payload_t rx_test_payload = ReceiveResponse {};
    auto& rx_test_response = std::get<ReceiveResponse>(rx_test_payload);

    auto& rx_radio_frame = *rx_test_response.mutable_frame();
    buf_unpack(rx_test_frame.buffer, rx_radio_frame);

    comm::serial::buffer_t rx_buffer;
    comm::serial::packet::encode(rx_test_response, rx_buffer);

    log::info("[HDLC Test] Encoded data:");
    log::info(vector_to_string(rx_buffer));

    comm::serial::hdlc_data_t rx_escaped_buffer;
    comm::serial::hdlc::escape(rx_buffer, rx_escaped_buffer);

    log::info("[HDLC Test] Escaped data:");
    log::info(vector_to_string(rx_escaped_buffer));

    comm::serial::hdlc_data_t rx_unescaped_buffer;
    comm::serial::hdlc::unescape(rx_escaped_buffer, rx_unescaped_buffer);

    log::info("[HDLC Test] Unescaped data:");
    log::info(vector_to_string(rx_unescaped_buffer));

    comm::serial::payload_t rx_final_payload = ReceiveResponse {};
    auto& rx_final_response = std::get<ReceiveResponse>(rx_final_payload);

    comm::serial::packet::decode(rx_unescaped_buffer, rx_final_payload);

    comm::mesh::frame rx_final_frame;
    buf_pack(rx_final_response.frame(), rx_final_frame.buffer);
    log::info("[HDLC Test] Output frame elements:");
    log::info(vector_to_string(rx_final_frame.buffer));

    if (rx_final_frame.buffer.size() != rx_test_frame.buffer.size()) {
        log::error("FAIL: RX frame size mismatch");
        return -1;
    }

    for (size_t i = 0; i < rx_final_frame.buffer.size(); ++i) {
        if (rx_final_frame.buffer[i] != rx_test_frame.buffer[i]) {
            log::error("FAIL: RX frame elements mismatch");
            return -1;
        }
    }

    log::info("[HDLC Test] [radio-client] PASSED");
    return 0;
}

static auto test_client_to_radio() -> int {
    log::info("[HDLC Test] Client to Radio test");

    comm::mesh::frame tx_test_frame;
    uint32_t tx_test_module = 1;

    tx_test_frame.buffer.resize(10);

    std::iota(tx_test_frame.buffer.begin(), tx_test_frame.buffer.end(), 11);

    log::info("[HDLC Test] Input frame elements:");
    log::info(vector_to_string(tx_test_frame.buffer));

    comm::serial::payload_t tx_test_payload = TransmitRequest {};
    auto& tx_test_request = std::get<TransmitRequest>(tx_test_payload);

    auto& tx_radio_frame = *tx_test_request.mutable_frame();
    buf_unpack(tx_test_frame.buffer, tx_radio_frame);
    tx_test_request.set_module(tx_test_module);

    comm::serial::buffer_t tx_buffer;
    comm::serial::packet::encode(tx_test_request, tx_buffer);

    log::info("[HDLC Test] Encoded data:");
    log::info(vector_to_string(tx_buffer));

    comm::serial::hdlc_data_t tx_escaped_buffer;
    comm::serial::hdlc::escape(tx_buffer, tx_escaped_buffer);

    log::info("[HDLC Test] Escaped data:");
    log::info(vector_to_string(tx_escaped_buffer));

    comm::serial::hdlc_data_t tx_unescaped_buffer;
    comm::serial::hdlc::unescape(tx_escaped_buffer, tx_unescaped_buffer);

    log::info("[HDLC Test] Unescaped data:");
    log::info(vector_to_string(tx_unescaped_buffer));

    comm::serial::payload_t tx_final_payload = TransmitRequest {};
    auto& tx_final_request = std::get<TransmitRequest>(tx_final_payload);

    comm::serial::packet::decode(tx_unescaped_buffer, tx_final_payload);

    comm::mesh::frame tx_final_frame;
    buf_pack(tx_final_request.frame(), tx_final_frame.buffer);
    log::info("[HDLC Test] Output frame elements:");
    log::info(vector_to_string(tx_final_frame.buffer));

    if (tx_final_frame.buffer.size() != tx_test_frame.buffer.size()) {
        log::error("FAIL: TX frame size mismatch");
        return -1;
    }

    for (size_t i = 0; i < tx_final_frame.buffer.size(); ++i) {
        if (tx_final_frame.buffer[i] != tx_test_frame.buffer[i]) {
            log::error("FAIL: TX frame elements mismatch");
            return -1;
        }
    }

    if (tx_final_request.module() != tx_test_module) {
        log::error("FAIL: TX module mismatch");
        return -1;
    }

    log::info("[HDLC Test] [client-radio] PASSED");
    return 0;
}

static auto test_special_hdlc_flags() -> int {
    log::info("[HDLC Test] Special HDLC Flags test");

    comm::serial::buffer_t test_buffer;
    test_buffer.resize(10);
    std::iota(test_buffer.begin(), test_buffer.end(), 1);

    test_buffer[4] = comm::serial::hdlc::flag;
    test_buffer[5] = comm::serial::hdlc::hdlc_esc;
    test_buffer[6] = comm::serial::hdlc::hdlc_esc_mask;

    log::info("[HDLC Test] Input buffer elements:");
    log::info(vector_to_string(test_buffer));

    comm::serial::hdlc_data_t escaped_buffer;
    comm::serial::hdlc::escape(test_buffer, escaped_buffer);

    log::info("[HDLC Test] Escaped data:");
    log::info(vector_to_string(escaped_buffer));

    comm::serial::hdlc_data_t unescaped_buffer;
    comm::serial::hdlc::unescape(escaped_buffer, unescaped_buffer);

    log::info("[HDLC Test] Unescaped data:");
    log::info(vector_to_string(unescaped_buffer));

    if (unescaped_buffer.size() != test_buffer.size()) {
        log::error("FAIL: Buffers size mismatch");
        return -1;
    }

    for (size_t i = 0; i < unescaped_buffer.size(); ++i) {
        if (unescaped_buffer[i] != test_buffer[i]) {
            log::error("FAIL: Buffers elements mismatch");
            return -1;
        }
    }

    log::info("[HDLC Test] [special-hdlc-flags] PASSED");
    return 0;
}

auto main(int argc, char** argv) noexcept -> int {
    int rc = 0;

    rc += test_radio_to_client();
    std::cout << std::endl;
    rc += test_client_to_radio();
    std::cout << std::endl;
    rc += test_special_hdlc_flags();

    return rc;
}
