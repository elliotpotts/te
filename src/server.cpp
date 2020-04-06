#include <te/server.hpp>
#include <te/app.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

te::server::server(std::uint16_t port, te::sim& model) :
    netio { SteamNetworkingSockets() },
    model { model } {
    listen(port);
    model.generate_map();
}

te::server::~server() {
    if (listen_sock != k_HSteamListenSocket_Invalid) {
        shutdown();
    }
}
    
void te::server::listen(std::uint16_t port) {
    SteamNetworkingIPAddr local_addr;
    local_addr.Clear();
    local_addr.m_port = port;
    listen_sock = netio->CreateListenSocketIP(local_addr, 0, nullptr);
    if (listen_sock == k_HSteamListenSocket_Invalid) {
        throw std::runtime_error{fmt::format("Failed to listen on port {}", port)};
    }
    poll_group = netio->CreatePollGroup();
    if (poll_group == k_HSteamNetPollGroup_Invalid) {
        throw std::runtime_error{fmt::format("Failed to create poll group on port {}", port)};
    }
    spdlog::info("Server listening on port {}", port);
}
    
void te::server::shutdown() {
    spdlog::info("Closing connections...");
    for (auto [conn, client] : net_clients) {
        netio->CloseConnection(conn, 0, "Server Shutdown", true /* linger */);
    }
    net_clients.clear();
    
    netio->CloseListenSocket(listen_sock);
    listen_sock = k_HSteamListenSocket_Invalid;

    netio->DestroyPollGroup(poll_group);
    poll_group = k_HSteamNetPollGroup_Invalid;
}
    
void te::server::send_bytes(HSteamNetConnection conn, std::span<const char> buffer) {
    netio->SendMessageToConnection(conn, buffer.data(), buffer.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void te::server::send_bytes_all(std::span<const char> buffer, HSteamNetConnection except) {
    for (auto [conn, client] : net_clients) {
        if (conn != except) {
            send_bytes(conn, buffer);
        }
    }
}

void te::server::run() {
}

void te::server::handle(client_handle& client, te::hello msg) {
    spdlog::debug("hello: family {}, {}", msg.family, msg.nick);
}

void te::server::handle(client_handle& client, msg_type type) {
    spdlog::error("Unknown message type {}", static_cast<int>(type));
}

std::unique_ptr<te::client> te::server::make_local(te::sim& model) {
    HSteamNetConnection server_end;
    HSteamNetConnection client_end;
    if (!netio->CreateSocketPair(&server_end, &client_end, false, nullptr, nullptr)) {
        spdlog::error("error creating pair! #1");
    }
    if (server_end == k_HSteamNetConnection_Invalid || client_end == k_HSteamNetConnection_Invalid) {
        spdlog::error("error creating pair! #2");
    }
    if (!netio->SetConnectionPollGroup(server_end, poll_group)) {
        spdlog::error("error setting poll group");
    }
    net_clients[server_end] = { .nick = "server" };
    return std::make_unique<te::net_client>(client_end, model);
}

void te::server::poll() {
    netio->RunCallbacks(this);
    ISteamNetworkingMessage* incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnPollGroup(poll_group, &incoming, 1);
    while (count_received == 1) {
        message_ptr received { incoming };
        auto client_it = net_clients.find(received->m_conn);
        deserialize (
            std::span{static_cast<const char*>(received->m_pData), received->m_cbSize},
            [&](const auto& msg) {
                handle(client_it->second, msg);
            }
        );
        count_received = netio->ReceiveMessagesOnPollGroup(poll_group, &incoming, 1);
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    }

    if (net_clients.size() == 2) {
        std::stringstream buffer;
        buffer.put(static_cast<unsigned char>(te::msg_type::full_update));
        {
            cereal::BinaryOutputArchive output { buffer };
            auto interesting = model.entities.view<te::named, te::site, te::footprint, te::render_mesh>();
            model.entities.snapshot().component<te::named, te::site, te::footprint, te::render_mesh>(output, interesting.begin(), interesting.end());
        }
        send_bytes_all(buffer.str());
    }
}

void te::server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_None:
        // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        // Ignore if they disconnected before we accepted the connection.
        if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
            // Locate the client.  Note that it should have been found, because this
            // is the only codepath where we remove clients (except on shutdown),
            // and connection change callbacks are dispatched in queue order.
            auto client_it = net_clients.find(info->m_hConn);
            assert(client_it != net_clients.end());

            // Select appropriate log messages
            const char* close_reason;
            if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                close_reason = "problem detected locally";
            } else {
                close_reason = "closed by peer";
            }
            spdlog::debug (
                "Connection {} {}, reason {}: {}",
                info->m_info.m_szConnectionDescription,
                close_reason,
                info->m_info.m_eEndReason,
                info->m_info.m_szEndDebug
            );
            net_clients.erase(client_it);
        } else {
            assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
        }
        // Do not linger because the connection is already closed on the other end
        netio->CloseConnection(info->m_hConn, 0, nullptr, false);
        break;
    }
    case k_ESteamNetworkingConnectionState_Connecting: {
        assert(net_clients.find(info->m_hConn) == net_clients.end()); // This must be a new connection
        spdlog::info("Connection request from {}", info->m_info.m_szConnectionDescription);
        if (netio->AcceptConnection(info->m_hConn) != k_EResultOK) {
            netio->CloseConnection(info->m_hConn, 0, nullptr, false);
            spdlog::info("Can't accept connection. (It was already closed?)");
            break;
        }

        // Assign the poll group
        if (!netio->SetConnectionPollGroup(info->m_hConn, poll_group)) {
            netio->CloseConnection(info->m_hConn, 0, nullptr, false);
            spdlog::info("Failed to set poll group?");
            break;
        }

        // Generate a random nick.
        std::string nick = fmt::format("BraveWarrior{}", 10000 + (rand() % 100000));
        spdlog::debug("{} joined", nick);

        net_clients.emplace(info->m_hConn, client_handle{0, nick});
        netio->SetConnectionName(info->m_hConn, nick.c_str());
        break;
    }
    case k_ESteamNetworkingConnectionState_Connected:
        break;

    default:
        break;
    }
}
