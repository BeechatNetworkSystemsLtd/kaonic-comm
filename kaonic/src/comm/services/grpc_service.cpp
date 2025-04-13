#include "kaonic/comm/services/grpc_service.hpp"

#include "kaonic/common/logging.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace kaonic::comm {

constexpr static auto pop_timeout = 50ms;

static auto grpc_buf_pack(const RadioFrame& src, std::vector<uint8_t>& dst) -> void {
    const auto& data = src.data();
    dst.resize(data.size() * sizeof(uint32_t));
    memcpy(dst.data(), data.data(), data.size() * sizeof(uint32_t));
    dst.resize(src.length());
}

static auto grpc_buf_unpack(const std::vector<uint8_t>& src, RadioFrame& dst) -> void {
    auto data = dst.mutable_data();

    size_t dst_size = src.size() / sizeof(uint32_t);
    dst_size += (src.size() - dst_size * sizeof(uint32_t)) ? 1 : 0;
    data->Resize(dst_size, 0);
    std::memcpy(data->mutable_data(), src.data(), src.size());

    dst.set_length(src.size());
}

grpc_radio_listener::grpc_radio_listener(const std::shared_ptr<grpc_service>& service) noexcept
    : _grpc_service { service } {}

auto grpc_radio_listener::on_receive(const mesh::frame& frame) -> void {
    _grpc_service->receive_frame(frame);
}

grpc_service::grpc_service(const std::shared_ptr<radio_service>& service,
                           std::string_view version) noexcept
    : Radio::Service {}
    , _radio_service { service }
    , _version { version } {}

auto grpc_service::Configure(::grpc::ServerContext* context,
                             const ConfigurationRequest* request,
                             Empty* response) -> ::grpc::Status {
    if (!_radio_service) {
        log::error("[GRPC service] Unable to configure radio: radio service wasn't initialized");
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                              "Unable to configure radio: radio service wasn't initialized");
    }

    const auto module = request->module();
    const auto freq = request->freq();
    const auto channel = request->channel();
    const auto channel_spacing = request->channel_spacing();
    const auto tx_power = request->tx_power();

    radio_phy_config_t phy_config = radio_phy_config_ofdm {};

    if (request->has_ofdm()) {
        phy_config = radio_phy_config_ofdm {
            .mcs = request->ofdm().mcs(),
            .opt = request->ofdm().opt(),
        };
    }

    radio_config config {
        .freq = freq,
        .channel = static_cast<uint8_t>(channel),
        .channel_spacing = channel_spacing,
        .phy_config = phy_config,
    };

    if (auto err = _radio_service->configure(module, config); !err.is_ok()) {
        log::error("[GRPC service] Unable to configure radio");
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to configure radio");
    }

    return ::grpc::Status::OK;
}

auto grpc_service::Transmit(::grpc::ServerContext* context,
                            const TransmitRequest* request,
                            TransmitResponse* response) -> ::grpc::Status {
    if (!_radio_service) {
        log::error("[GRPC service] Unable to transmit: radio service wasn't initialized");
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                              "Unable to transmit: radio service wasn't initialized");
    }
    const auto& module = request->module();
    const auto& frame = request->frame();

    grpc_buf_pack(frame, _tx_frame.buffer);

    auto err = _radio_service->transmit(module, _tx_frame);

    if (!err.is_ok()) {
        log::error("[GRPC service] Unable to transmit");
        return ::grpc::Status(::grpc::StatusCode::INTERNAL, "Unable to transmit");
    }

    return ::grpc::Status::OK;
}

auto grpc_service::ReceiveStream(::grpc::ServerContext* context,
                                 const ReceiveRequest* request,
                                 ::grpc::ServerWriter<ReceiveResponse>* writer) -> ::grpc::Status {
    if (!_radio_service) {
        log::error("[GRPC service] Unable to set receive stream: radio service wasn't initialized");
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                              "Unable to set receive stream: radio service wasn't initialized");
    }

    log::info("grpc: start receive stream");

    ReceiveResponse response;

    auto frame = response.mutable_frame();

    writer->Write(response);

    while (context && !context->IsCancelled()) {

        if (!pop_frame(_rx_frame, pop_timeout)) {
            continue;
        }

        grpc_buf_unpack(_rx_frame.buffer, *frame);

        if (!writer || !writer->Write(response)) {
            log::error("[Radio Service] Unable to write to the client stream");
            return ::grpc::Status(::grpc::StatusCode::ABORTED,
                                  "Unable to write to the client stream");
        }
    }

    log::debug("grpc: stop receive stream");

    return ::grpc::Status::OK;
}

auto grpc_service::receive_frame(const mesh::frame& frame) -> void {
    std::unique_lock<std::mutex> lock(_mut);

    if (_frame_queue.size() >= 64) {
        _frame_queue.pop();
    }

    _frame_queue.push(frame);
    _frame_queue_cond.notify_one();
}

auto grpc_service::pop_frame(mesh::frame& frame, std::chrono::milliseconds timeout) -> bool {
    std::unique_lock<std::mutex> lock(_mut);

    if (!_frame_queue_cond.wait_for(lock, timeout, [this] { return !_frame_queue.empty(); })) {
        return false;
    }

    frame = _frame_queue.front();
    _frame_queue.pop();

    return true;
}

} // namespace kaonic::comm
