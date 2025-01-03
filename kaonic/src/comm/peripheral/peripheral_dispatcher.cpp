#include "kaonic/comm/peripheral/peripheral_dispatcher.hpp"

#include "kaonic/common/logging.hpp"

namespace kaonic::peripheral {

peripheral_dispatcher::~peripheral_dispatcher() {
    for (auto& peripheral : _peripherals) {
        peripheral->close();
    }
}

auto peripheral_dispatcher::attach(std::unique_ptr<peripheral>&& peripheral) -> error {
    _peripherals.push_back(std::move(peripheral));
    return error::ok();
}

auto peripheral_dispatcher::broadcast(const protocols::hdlc_payload_t& payload) -> error {
    const auto frame = protocols::hdlc::serialize(payload);
    for (auto& peripheral : _peripherals) {
        if (auto err = peripheral->write(frame); !err.is_ok()) {
            log::error("[Peripheral Dispatcher] Unable to write frame");
            return err;
        }
    }
    return error::ok();
}

auto peripheral_dispatcher::listen(protocols::hdlc_payload_t& payload) -> error {
    protocols::hdlc_frame_t frame {};

    for (auto& peripheral : _peripherals) {
        if (auto err = peripheral->read(frame); !err.is_ok()) {
            log::error("[Peripheral Dispatcher] Unable to read frame");
            return err;
        }

        if (frame.size() == 0) {
            continue;
        }

        payload = protocols::hdlc::deserialize(frame);
        if (payload.size() == 0) {
            log::error("[Peripheral Dispatcher] Empty or corrupted payload from the peripheral");
            return error::fail();
        }

        break;
    }
    return error::ok();
}

} // namespace kaonic::peripheral
