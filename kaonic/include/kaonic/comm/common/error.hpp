#pragma once

namespace kaonic {

enum class error_code {
    ok,
    fail,
    invalid_arg,
    precondition_failed,
};

struct error final {

    error_code code = error_code::ok;

    constexpr error() = default;

    constexpr explicit error(error_code code)
        : code { code } {}

    constexpr error(const error&) = default;
    constexpr error(error&&) noexcept = default;

    constexpr error& operator=(const error&) = default;
    constexpr error& operator=(error&&) noexcept = default;

    [[nodiscard]] constexpr auto is_ok() const noexcept -> bool { return code == error_code::ok; }

    [[nodiscard]] static constexpr auto ok() noexcept -> error { return error { error_code::ok }; }

    [[nodiscard]] static constexpr auto fail() noexcept -> error {
        return error { error_code::fail };
    }

    [[nodiscard]] static constexpr auto invalid_arg() noexcept -> error {
        return error { error_code::invalid_arg };
    }

    [[nodiscard]] static constexpr auto precondition_failed() noexcept -> error {
        return error { error_code::precondition_failed };
    }

    constexpr auto with(const error& err) noexcept -> void {
        if (!is_ok() || !err.is_ok()) {
            code = error_code::fail;
        }
    }

    constexpr auto operator+=(const error& err) noexcept -> error& {
        with(err);
        return *this;
    }

    [[nodiscard]] constexpr auto to_str() const noexcept -> const char* { return as_str(code); }

    [[nodiscard]] static constexpr auto as_str(error_code code) noexcept -> const char* {
        switch (code) {
            case error_code::ok:
                return "ok";
            case error_code::fail:
                return "fail";
            case error_code::invalid_arg:
                return "invalid arg";
            case error_code::precondition_failed:
                return "precondition_failed";
            default:
                return "unknown";
        }
    }
};

} // namespace kaonic
