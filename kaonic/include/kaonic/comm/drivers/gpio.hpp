#pragma once

#include <cstdint>
#include <string_view>

namespace kaonic::drivers {

struct gpio_spec {
    std::string_view gpio_chip;
    size_t gpio_line = 0;
};

} // namespace kaonic::drivers
