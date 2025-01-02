#pragma once

#include "spdlog/spdlog.h"

namespace kaonic {

struct log final {

    using level = spdlog::level::level_enum;

    static auto set_level(level level) noexcept -> void { spdlog::set_level(level); }

    template <class T, class... Args>
    static auto trace(T fmt, Args&&... args) noexcept -> void {
        spdlog::trace(fmt, args...);
    }

    template <class T, class... Args>
    static auto debug(T fmt, Args&&... args) noexcept -> void {
        spdlog::debug(fmt, args...);
    }

    template <class T, class... Args>
    static auto info(T fmt, Args&&... args) noexcept -> void {
        spdlog::info(fmt, args...);
    }

    template <class T, class... Args>
    static auto warn(T fmt, Args&&... args) noexcept -> void {
        spdlog::warn(fmt, args...);
    }

    template <class T, class... Args>
    static auto error(T fmt, Args&&... args) noexcept -> void {
        spdlog::critical(fmt, args...);
    }
};

} // namespace kaonic
