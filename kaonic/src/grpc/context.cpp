#include "kaonic/grpc/context.hpp"

namespace kaonic::grpc {

grpc::context::context(std::unique_ptr<comm::radio> radio, const std::string& version)
    : _radio { std::move(radio) }
    , _version { version } {}

context::~context() {
    if (_radio) {
        _radio->close();
    }
}

auto context::get_radio() const -> comm::radio& {
    return *_radio;
}

auto kaonic::grpc::context::get_version() const -> std::string {
    return _version;
}

} // namespace kaonic::grpc
