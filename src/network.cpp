#include <te/net.hpp>
#include <spdlog/spdlog.h>

void te::message_deleter::operator()(ISteamNetworkingMessage* msg) const {
    msg->Release();
};
