#pragma once

#include <memory>
#include <string>

#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::grpc {

class context {

public:
    explicit context(std::unique_ptr<comm::radio> radio, const std::string& version);
    ~context();

    [[nodiscard]] auto get_radio() const -> comm::radio&;

    [[nodiscard]] auto get_version() const -> std::string;

protected:
    context(const context&) = delete;
    context(context&&) noexcept = delete;

    context& operator=(const context&) = delete;
    context& operator=(context&&) noexcept = delete;

private:
    std::unique_ptr<comm::radio> _radio = nullptr;

    std::string _version;
};

} // namespace kaonic::grpc
