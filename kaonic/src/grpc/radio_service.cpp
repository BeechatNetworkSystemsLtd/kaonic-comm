#include "kaonic/grpc/radio_service.hpp"

#include "kaonic/comm/common/logging.hpp"

namespace kaonic::grpc {

constexpr static auto trx_to_enum(TrxType type) noexcept -> comm::trx_type {
    switch (type) {
        case TrxType::RF09:
            return comm::trx_type::rf09;
        case TrxType::RF24:
            return comm::trx_type::rf24;
        default:
            return comm::trx_type::rf09;
    }
}

radio_service::radio_service(const std::shared_ptr<context>& context)
    : Radio::Service {}
    , _context { context } {}

auto radio_service::Configure(::grpc::ServerContext* context,
                              const ConfigurationRequest* request,
                              Empty* response) -> ::grpc::Status {
    comm::radio_config config = {
        .mobule = static_cast<uint8_t>(request->module()),
        .type = trx_to_enum(request->trx_type()),
        .freq = request->freq(),
        .channel = request->channel(),
        .channel_spacing = request->channel_spacing(),
    };

    auto& radio = _context->get_radio();

    if (auto err = radio.set_config(config); !err.is_ok()) {
        log::error("[Radio Service] Unable to set config: {}", err.to_str());
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to set config");
    }
    return ::grpc::Status::OK;
}

auto kaonic::grpc::radio_service::Transmit(::grpc::ServerContext* context,
                                           const TransmitRequest* request,
                                           TransmitResponse* response) -> ::grpc::Status {
    const auto& frame = request->frame();
    const auto& data = frame.data();
    const auto data_size = frame.length();

    auto& radio = _context->get_radio();

    comm::tx_packet tx_packet = {
        .mobule = static_cast<uint8_t>(request->module()),
        .type = trx_to_enum(request->trx_type()),
        .frame = {
            .len = data_size,
            .crc = frame.crc32(),
        },
    };

    uint32_t pos = 0;
    uint32_t val = 0;
    for (size_t i = 0; i < data_size; ++i) {
        pos = i % 4;
        val = data[pos];
        tx_packet.frame.data[i] = static_cast<uint8_t>((val >> (pos * 8)) & 0xff);
    }

    comm::tx_response tx_response = { 0 };
    if (auto err = radio.transmit(tx_packet, tx_response); !err.is_ok()) {
        log::error("[Radio Service] Unable to transmit packet: {}", err.to_str());
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to transmit packet");
    }

    response->set_latency(tx_response.latency);
    return ::grpc::Status::OK;
}

auto kaonic::grpc::radio_service::Receive(::grpc::ServerContext* context,
                                          const ReceiveRequest* request,
                                          ReceiveResponse* response) -> ::grpc::Status {
    auto& radio = _context->get_radio();

    comm::rx_request rx_request = {
        .module = static_cast<uint8_t>(request->module()),
        .type = trx_to_enum(request->trx_type()),
        .timeout = request->timeout(),
    };

    comm::rx_packet rx_packet = { 0 };
    if (auto err = radio.recieve(rx_request, rx_packet); !err.is_ok()) {
        log::error("[Radio Service] Unable to recieve packet: {}", err.to_str());
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to recieve packet");
    }

    response->set_latency(rx_packet.latency);
    response->set_rssi(rx_packet.rssi);
    auto frame = response->mutable_frame();

    frame->set_length(rx_packet.frame.len);
    frame->set_crc32(rx_packet.frame.crc);

    uint32_t val = 0;
    for (size_t i = 0; i < rx_packet.frame.len; i += 4) {
        val != (i < rx_packet.frame.len) ? rx_packet.frame.data[i] : 0;
        val != (i + 1 < rx_packet.frame.len) ? rx_packet.frame.data[i + 1] << 8 : 0;
        val != (i + 2 < rx_packet.frame.len) ? rx_packet.frame.data[i + 2] << 16 : 0;
        val != (i + 3 < rx_packet.frame.len) ? rx_packet.frame.data[i + 3] << 24 : 0;
        frame->add_data(val);
    }

    return ::grpc::Status::OK;
}

auto kaonic::grpc::radio_service::ReceiveStream(::grpc::ServerContext* context,
                                                const Empty* request,
                                                ::grpc::ServerWriter<RadioFrame>* writer)
    -> ::grpc::Status {
    return ::grpc::Status::OK;
}

} // namespace kaonic::grpc
