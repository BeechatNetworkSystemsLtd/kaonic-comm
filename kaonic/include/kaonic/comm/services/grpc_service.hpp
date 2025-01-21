#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>

#include "kaonic/comm/services/radio_service.hpp"

#include <radio.grpc.pb.h>

#include <grpc/grpc.h>

namespace kaonic::comm {

class grpc_service final : public Radio::Service {

public:
    explicit grpc_service(const std::shared_ptr<radio_service>& service,
                          std::string_view version) noexcept;

    grpc_service(const grpc_service&) = delete;
    grpc_service(grpc_service&&) noexcept = delete;

    [[nodiscard]] auto Configure(::grpc::ServerContext* context,
                                 const ConfigurationRequest* request,
                                 Empty* response) -> ::grpc::Status final;

    [[nodiscard]] auto Transmit(::grpc::ServerContext* context,
                                const TransmitRequest* request,
                                TransmitResponse* response) -> ::grpc::Status final;

    [[nodiscard]] auto ReceiveStream(::grpc::ServerContext* context,
                                     const ReceiveRequest* request,
                                     ::grpc::ServerWriter<ReceiveResponse>* writer)
        -> ::grpc::Status final;

    auto receive_frame(const mesh::frame& frame) -> void;

    grpc_service& operator=(const grpc_service&) = delete;
    grpc_service& operator=(grpc_service&&) noexcept = delete;

private:
    auto pop_frame(mesh::frame& frame, std::chrono::milliseconds timeout) -> bool;

private:
    std::shared_ptr<radio_service> _radio_service;

    std::string_view _version;

    std::queue<mesh::frame> _frame_queue;
    mutable std::mutex _mut;
    std::condition_variable _frame_queue_cond;

    mesh::frame _tx_frame;
    mesh::frame _rx_frame;
};

class grpc_radio_listener final : public mesh::network_receiver {

public:
    explicit grpc_radio_listener(const std::shared_ptr<grpc_service>& service) noexcept;
    ~grpc_radio_listener() final = default;

    auto on_receive(const mesh::frame& frame) -> void final;

private:
    std::shared_ptr<grpc_service> _grpc_service;
};

} // namespace kaonic::comm
