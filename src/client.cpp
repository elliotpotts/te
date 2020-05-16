#include <te/client.hpp>
#include <te/app.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

HSteamNetConnection te::client::connect_to_addr(ISteamNetworkingSockets* netio, const SteamNetworkingIPAddr& server_addr) {
    std::array<char, SteamNetworkingIPAddr::k_cchMaxString> addr_string;
    server_addr.ToString(addr_string.data(), sizeof(addr_string), true);
    spdlog::info("Connecting to server at {}", addr_string.data());
    HSteamNetConnection conn = netio->ConnectByIPAddress(server_addr, 0, nullptr);
    if (conn == k_HSteamNetConnection_Invalid) {
        throw std::runtime_error{"Failed to create connection"};
    } else {
        return conn;
    }
}

te::client::client(ISteamNetworkingSockets* netio, HSteamNetConnection conn, te::sim& model):
    netio { netio },
    conn { conn },
    model { model } {
}

te::client::client(ISteamNetworkingSockets* netio, const SteamNetworkingIPAddr& server_addr, te::sim& model):
    netio { SteamNetworkingSockets() },
    conn { connect_to_addr(netio, server_addr) },
    model { model } {
}

te::client::client(client&& rhs):
    netio { rhs.netio },
    conn { rhs.conn },
    model { rhs.model } {
    rhs.conn = k_HSteamNetConnection_Invalid;
}

te::client::~client() {
    if (conn != k_HSteamNetConnection_Invalid) {
        netio->CloseConnection(conn, 0, "quit", true);
    }
}

void te::client::handle(te::hello msg) {
    spdlog::debug("got hello from server!");
    my_family = msg.family;
    my_nick = msg.nick;
}
void te::client::handle(te::chat msg) {
    on_chat(msg);
}
void te::client::handle(te::entity_create msg) {
}
void te::client::handle(te::entity_delete msg) {
}
void te::client::handle(te::component_replace msg) {
}

namespace {
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
}
void te::client::poll() {
    netio->RunCallbacks(this);
    ISteamNetworkingMessage* incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    while (count_received == 1) {
        message_ptr received { incoming };
        std::span payload {
            static_cast<const char*>(received->m_pData),
            static_cast<std::size_t>(received->m_cbSize)
        };
        auto received_msg = te::deserialized<te::msg>(payload);
        std::visit([&](auto& m) { handle(m); }, received_msg);
        count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    }
}

std::optional<unsigned> te::client::family() {
    return my_family;
}

std::optional<std::string> te::client::nick() {
    return my_nick;
}

void te::client::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    if (info->m_hConn != conn) {
        // this status change is probably intended for a server on the same host as the client
        return;
    }
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_None:
        break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
            spdlog::info("Couldn't connect to server: {}", info->m_info.m_szEndDebug);
        } else if ( info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally ) {
            spdlog::info("Lost connection to server: {}", info->m_info.m_szEndDebug);
        } else {
            spdlog::info("Closed by peer (?): {}", info->m_info.m_szEndDebug);
        }
        netio->CloseConnection(info->m_hConn, 0, nullptr, false);
        conn = k_HSteamNetConnection_Invalid;
        break;

    case k_ESteamNetworkingConnectionState_Connecting:
        spdlog::error("we're getting this in the client but should be getting it in the server :(");
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        spdlog::info("Connected to server");
        break;
    default:
        break;
    }
}

void te::client::send(std::span<const std::byte> buffer) {
    netio->SendMessageToConnection(conn, buffer.data(), buffer.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void te::client::send(te::msg&& m) {
    std::string as_str = serialized(m);
    std::span<const std::byte> as_byte_span {reinterpret_cast<const std::byte*>(std::to_address(as_str.begin())), as_str.size()};
    send(as_byte_span);
}
