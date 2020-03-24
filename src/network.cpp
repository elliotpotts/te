#include <te/net.hpp>
#include <spdlog/spdlog.h>

void te::network_debug_output(ESteamNetworkingSocketsDebugOutputType type, const char* message) {
    spdlog::debug(message);
    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        throw std::runtime_error("That was a bug! Bye-bye ;)");
    }
}

void te::message_deleter::operator()(ISteamNetworkingMessage* msg) const {
    msg->Release();
};
