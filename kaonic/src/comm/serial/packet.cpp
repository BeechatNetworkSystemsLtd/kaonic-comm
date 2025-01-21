#include "kaonic/comm/serial/packet.hpp"

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::serial {

constexpr static uint16_t magic = 0x22;

enum class packet_type : uint16_t {
    unknown = 0,
    config = 1,
    transmit = 2,
    receive = 3,
};

struct packet_header final {
    uint16_t magic;
    packet_type type;
    uint8_t reserved[16];
};

auto packet::decode(const buffer_t& buffer, payload_t& payload) noexcept -> void {
    if (buffer.size() < sizeof(packet_header)) {
        payload = error_payload {};
        return;
    }

    packet_header header;
    memcpy(&header, buffer.data(), sizeof(header));

    if (header.magic != magic) {
        payload = error_payload {};
        return;
    }

    switch (header.type) {
        case packet_type::config: {
            ConfigurationRequest config;
            if (!config.ParseFromArray(buffer.data() + sizeof(header),
                                       buffer.size() - sizeof(header))) {
                log::warn("[Serial Service] TX failed: unable to parse config packet");
                payload = error_payload {};
                break;
            }
            payload = config;
            break;
        }
        case packet_type::transmit: {
            TransmitRequest request;
            if (!request.ParseFromArray(buffer.data() + sizeof(header),
                                        buffer.size() - sizeof(header))) {
                log::warn("[Serial Service] TX failed: unable to parse frame packet");
                payload = error_payload {};
                break;
            }
            payload = request;
            break;
        }
        case packet_type::receive: {
            ReceiveResponse response;
            if (!response.ParseFromArray(buffer.data() + sizeof(header),
                                         buffer.size() - sizeof(header))) {
                log::warn("[Serial Service] RX failed: unable to parse frame packet");
                payload = error_payload {};
                break;
            }
            payload = response;
            break;
        }
        default: {
            payload = error_payload {};
            break;
        }
    }
}

auto serial::packet::encode(const payload_t& payload, buffer_t& buffer) noexcept -> void {
    std::visit(
        [&buffer](auto&& payload) {
            using T = std::decay_t<decltype(payload)>;

            packet_header header;
            header.magic = magic;

            size_t size = 0;
            buffer.clear();
            if constexpr (std::is_same_v<T, ConfigurationRequest>) {
                header.type = packet_type::config;
                size = payload.ByteSizeLong();
                buffer.resize(size + sizeof(header));
                memcpy(buffer.data(), &header, sizeof(header));
                payload.SerializeToArray(buffer.data() + sizeof(header), static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, TransmitRequest>) {
                header.type = packet_type::transmit;
                size = payload.ByteSizeLong();
                buffer.resize(size + sizeof(header));
                memcpy(buffer.data(), &header, sizeof(header));
                payload.SerializeToArray(buffer.data() + sizeof(header), static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, ReceiveResponse>) {
                header.type = packet_type::receive;
                size = payload.ByteSizeLong();
                buffer.resize(size + sizeof(header));
                memcpy(buffer.data(), &header, sizeof(header));
                payload.SerializeToArray(buffer.data() + sizeof(header), static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, error_payload>) {
                header.type = packet_type::unknown;
                buffer.resize(sizeof(header));
                memcpy(buffer.data(), &header, sizeof(header));
            }
        },
        payload);
}

} // namespace kaonic::comm::serial
