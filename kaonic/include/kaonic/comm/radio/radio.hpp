#pragma once

#include <chrono>
#include <cstdint>
#include <variant>

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/common/error.hpp"

constexpr auto data_max_size = 2048u;

namespace kaonic::comm {

struct radio_frame final {
    uint16_t len;
    uint8_t data[data_max_size];
};

struct radio_phy_config_ofdm {
    uint32_t mcs = 6;
    uint32_t opt = 0;
};

struct radio_phy_config_fsk {
    // FSK Bandwidth Time Product
    uint8_t bt = 0x03;
    // FSK Modulation Index Scale
    uint8_t midxs = 0x01;
    // FSK Modulation Index
    uint8_t midx = 0x03;
    // FSK Modulation Order
    uint8_t mord = 0x00;

    uint16_t preamble_length = 0;

    bool freq_inversion = false;

    // MR-FSK Symbol Rate
    uint8_t srate = 0x00;

    // Preamble Detection Mode
    uint8_t pdtm = 0x00;

    // Receiver Override
    uint8_t rxo = 0x02;

    // Receiver Preamble Time Out
    uint8_t rxpto = 0x00;

    // Mode Switch Enable
    uint8_t mse = 0x00;

    bool preamble_inversion = false;

    // FEC Scheme
    uint8_t fecs = 0x00;

    // FEC Interleaving Enable
    bool fecie = true;

    // SFD Detection Threshold
    uint8_t sfdt = 0x08;

    // Preamble Detection Threshold
    uint8_t pdt = 0x05;

    // SFD Quantization
    bool sftq = false;

    // SFD 32Bit
    uint8_t sfd32 = 0;

    bool rawbit = true;

    // Configuration of PPDU with SFD1
    uint8_t csfd1 = 0x02;

    // Configuration of PPDU with SFD0
    uint8_t csfd0 = 0x00;

    uint16_t sfd0 = 0x7209;
    uint16_t sfd1 = 0x72F6;

    uint8_t sfd = 0x00;

    // Data Whitening
    uint8_t dw = 0x01;

    // FSK Preemphasis
    bool pe = false;
    // FSK Direct Modulation Enable
    bool en = false;

    uint8_t fskpe0 = 0x00;
    uint8_t fskpe1 = 0x00;
    uint8_t fskpe2 = 0x00;
};

using radio_phy_config_t = std::variant<radio_phy_config_ofdm, radio_phy_config_fsk>;

struct radio_config final {
    uint32_t freq = 869535;
    uint8_t channel = 1;
    uint32_t channel_spacing = 200;
    uint32_t tx_power = 10;
    radio_phy_config_t phy_config = radio_phy_config_ofdm {};
};

class radio {

public:
    virtual ~radio() = default;

    virtual auto configure(const radio_config& config) -> error = 0;

    virtual auto transmit(const radio_frame& frame) -> error = 0;

    virtual auto receive(radio_frame& frame, const std::chrono::milliseconds& timeout) -> error = 0;

protected:
    explicit radio() = default;

    radio(const radio&) = default;
    radio(radio&&) = default;

    radio& operator=(const radio&) = default;
    radio& operator=(radio&&) = default;
};

} // namespace kaonic::comm
