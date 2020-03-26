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

te::client::client(const SteamNetworkingIPAddr& server_addr, te::sim& model):
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

te::client::~client() {
    netio->CloseConnection(conn, 0, "quit", true);
}

std::vector<te::message_ptr> te::client::recv() {
    std::vector<te::message_ptr> received;
    ISteamNetworkingMessage *incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    while (count_received == 1) {
        received.emplace_back(incoming);
        count_received = netio->ReceiveMessagesOnConnection(conn, &incoming, 1);
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    } else {
        return received;
    }
}

void te::client::poll() {
    netio->RunCallbacks(this);
    std::vector<te::message_ptr> received = recv();
    for (auto& msg_ptr : received) {
        std::stringstream strmsg;
        auto strmsg_writer = std::ostreambuf_iterator { strmsg.rdbuf() };
        std::copy_n(static_cast<char*>(msg_ptr->m_pData), msg_ptr->m_cbSize, strmsg_writer);
        cereal::BinaryInputArchive input { strmsg };
        model.entities.loader().component<te::named, te::site, te::footprint, te::render_mesh>(input);
    }
}

void te::client::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    assert(info->m_hConn == conn || conn == k_HSteamNetConnection_Invalid);
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_None:
        // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        if ( info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
            // Note: we could distinguish between a timeout, a rejected connection, or some other transport problem.
            spdlog::info("We sought the remote host, yet our efforts were met with defeat. ({})", info->m_info.m_szEndDebug);
        } else if ( info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally ) {
            spdlog::info("Alas, troubles beset us; we have lost contact with the host. ({})", info->m_info.m_szEndDebug);
        } else {
            spdlog::info("The host hath bidden us farewell. ({})", info->m_info.m_szEndDebug);
        }
        netio->CloseConnection(info->m_hConn, 0, nullptr, false);
        conn = k_HSteamNetConnection_Invalid;
        break;

    case k_ESteamNetworkingConnectionState_Connecting:
        // We will get this callback when we start connecting. We can ignore this.
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        spdlog::info("Connected to server OK");
        break;

    default:
        break;
    }
}

void te::client::send(std::vector<char> buffer) {
    netio->SendMessageToConnection(conn, buffer.data(), buffer.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}
