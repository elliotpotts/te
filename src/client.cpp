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

te::client::~client() {
}

te::net_client::net_client(const SteamNetworkingIPAddr& server_addr, te::sim& model):
    netio { SteamNetworkingSockets() },
    model { model } {
    std::array<char, SteamNetworkingIPAddr::k_cchMaxString> addr_string;
    server_addr.ToString(addr_string.data(), sizeof(addr_string), true);
    spdlog::info("Connecting to chat server at {}", addr_string.data());
    conn = netio->ConnectByIPAddress(server_addr, 0, nullptr);
    if (conn == k_HSteamNetConnection_Invalid) {
        throw std::runtime_error{"Failed to create connection"};
    }
}

te::net_client::net_client(HSteamNetConnection conn, te::sim& model):
    netio { SteamNetworkingSockets() },
    model { model },
    conn { conn } {
}

te::net_client::~net_client() {
    netio->CloseConnection(conn, 0, "quit", true);
}

void te::net_client::handle(te::hello msg) {
    spdlog::debug("got hello from server!");
    my_family = msg.family;
}

void te::net_client::handle(te::full_update update) {
    std::stringstream strmsg;
    auto strmsg_writer = std::ostreambuf_iterator { strmsg.rdbuf() };
    std::copy(update.entities.begin(), update.entities.end(), strmsg_writer);
    {
        cereal::BinaryInputArchive input { strmsg };
        model.entities.loader().component<te::owned, te::named, te::price, te::footprint, te::site, te::demander, te::generator, te::producer, te::inventory, te::market, te::merchant, te::render_tex, te::render_mesh, te::noisy, te::pickable>(input);
    }
}

void te::net_client::handle(te::msg_type type) {
    spdlog::error("Unknown message type {}", static_cast<int>(type));
}

void te::net_client::poll() {
    netio->RunCallbacks(this);
    ISteamNetworkingMessage* incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    while (count_received == 1) {
        message_ptr received { incoming };
        deserialize_to (
            std::span{static_cast<const char*>(received->m_pData), static_cast<std::size_t>(received->m_cbSize)},
            [&](const auto& msg) {
                handle(msg);
            }
        );
        count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    }
}

std::optional<unsigned> te::net_client::family() {
    return my_family;
}

void te::net_client::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    if (info->m_hConn != conn) {
        spdlog::error("Something spooky happened");
        //assert(info->m_hConn == conn || conn == k_HSteamNetConnection_Invalid);
    }
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_None:
        // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
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
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        spdlog::info("Connected to server");
        break;
    default:
        break;
    }
}

void te::net_client::send(std::span<const std::byte> buffer) {
    netio->SendMessageToConnection(conn, buffer.data(), buffer.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}
