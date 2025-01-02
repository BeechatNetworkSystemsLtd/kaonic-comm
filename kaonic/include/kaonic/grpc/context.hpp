#pragma once

#include <memory>
#include <string_view>

#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::grpc {

class context {

public:
    explicit context(std::unique_ptr<comm::radio> radio, std::string_view version);
    ~context() = default;

    [[nodiscard]] auto get_radio() const -> comm::radio&;

    [[nodiscard]] auto get_version() const -> std::string_view;

protected:
    context(const context&) = delete;
    context(context&&) noexcept = delete;

    context& operator=(const context&) = delete;
    context& operator=(context&&) noexcept = delete;

private:
    std::unique_ptr<comm::radio> _radio;

    std::string_view _version;
};

} // namespace kaonic::grpc
