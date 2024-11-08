#include "kaonic/grpc/radio_service.hpp"

#include <cstring>

#include "kaonic/comm/common/logging.hpp"

namespace kaonic::grpc {

static auto grpc_buf_pack(const google::protobuf::RepeatedField<uint32_t>& src, uint8_t dst[])
    -> void {
    std::memcpy(dst, src.data(), src.size() * sizeof(uint32_t));
}

static auto
grpc_buf_unpack(uint8_t src[], google::protobuf::RepeatedField<uint32_t>* dst, size_t len) -> void {
    size_t dst_size = len / sizeof(uint32_t);
    dst_size += (len - dst_size * sizeof(uint32_t)) ? 1 : 0;
    dst->Resize(dst_size, 0);
    std::memcpy(dst->begin(), src, len);
}

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

    if (auto err = radio.configure(config); !err.is_ok()) {
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

    comm::tx_request tx_request = {
        .mobule = static_cast<uint8_t>(request->module()),
        .type = trx_to_enum(request->trx_type()),
        .frame = {
            .len = data_size,
            .crc = frame.crc32(),
        },
    };
    grpc_buf_pack(data, tx_request.frame.data);

    comm::tx_response tx_response = { 0 };
    if (auto err = radio.transmit(tx_request, tx_response); !err.is_ok()) {
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

    comm::rx_response rx_response = { 0 };
    if (auto err = radio.recieve(rx_request, rx_response); !err.is_ok()) {
        log::error("[Radio Service] Unable to recieve packet: {}", err.to_str());
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to recieve packet");
    }

    response->set_latency(rx_response.latency);
    response->set_rssi(rx_response.rssi);
    auto frame = response->mutable_frame();

    auto& resp_frame = rx_response.frame;
    frame->set_length(resp_frame.len);
    frame->set_crc32(resp_frame.crc);

    auto data = frame->mutable_data();
    grpc_buf_unpack(resp_frame.data, data, resp_frame.len);

    return ::grpc::Status::OK;
}

auto kaonic::grpc::radio_service::ReceiveStream(::grpc::ServerContext* context,
                                                const ReceiveRequest* request,
                                                ::grpc::ServerWriter<ReceiveResponse>* writer)
    -> ::grpc::Status {
    auto& radio = _context->get_radio();

    comm::rx_request rx_request = {
        .module = static_cast<uint8_t>(request->module()),
        .type = trx_to_enum(request->trx_type()),
        .timeout = request->timeout(),
    };

    comm::rx_response rx_response = { 0 };
    error err {};

    ReceiveResponse response;
    while (context && !context->IsCancelled()) {
        err = radio.recieve(rx_request, rx_response);
        switch (err.code) {
            case error_code::ok:
                break;
            case error_code::timeout:
                log::warn("[Radio Service] Radio RX timeout exceed");
                continue;
            default:
                log::error("[Radio Service] Unable to recieve packet: {}", err.to_str());
                return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to recieve packet");
        }

        response.set_latency(rx_response.latency);
        response.set_rssi(rx_response.rssi);
        auto frame = response.mutable_frame();

        auto& resp_frame = rx_response.frame;
        frame->set_length(resp_frame.len);
        frame->set_crc32(resp_frame.crc);

        auto data = frame->mutable_data();
        grpc_buf_unpack(resp_frame.data, data, resp_frame.len);

        if (!writer || !writer->Write(response)) {
            log::error("[Radio Service] Unable to write to the client stream");
            return ::grpc::Status(::grpc::StatusCode::ABORTED,
                                  "Unable to write to the client stream");
        }
    }
    return ::grpc::Status::OK;
}

} // namespace kaonic::grpc
