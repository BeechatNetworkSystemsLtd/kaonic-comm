#include "kaonic/grpc/context.hpp"

namespace kaonic::grpc {

grpc::context::context(std::unique_ptr<comm::radio> radio, std::string_view version)
    : _radio { std::move(radio) }
    , _version { version } {}

auto context::get_radio() const -> comm::radio& {
    return *_radio;
}

auto kaonic::grpc::context::get_version() const -> std::string_view {
    return _version;
}

} // namespace kaonic::grpc
