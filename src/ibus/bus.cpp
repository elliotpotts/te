#include <ibus/bus.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

ibus::bus::bus() : hnd(ibus_bus_new()) {
    //TODO: gracefuly degrade to non-ime input only
    if (!ibus_bus_is_connected(hnd)) {
        throw std::runtime_error("Couldn't connect to IBus!");
    }
}

IBusInputContext* ibus::bus::make_context() {
    IBusInputContext* ic = ibus_bus_create_input_context(hnd, "trade-empires");
    if (!ic) {
        throw std::runtime_error("Couldn't create IBus input context");
    } else {
        return ic;
    }
}
