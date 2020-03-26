#include <te/server.hpp>
#include <te/app.hpp>
#include <spdlog/spdlog.h>

te::server::server(te::sim& model) :
    netio { SteamNetworkingSockets() },
    model { model } {
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
    for (auto [conn, client] : conn_clients) {
        send(conn, "Server is shutting down. Goodbye.");
        // Use linger mode to gracefully close the connection
        netio->CloseConnection(conn, 0, "Server Shutdown", true);
    }
    conn_clients.clear();

    netio->CloseListenSocket(listen_sock);
    listen_sock = k_HSteamListenSocket_Invalid;

    netio->DestroyPollGroup(poll_group);
    poll_group = k_HSteamNetPollGroup_Invalid;
}
    
void te::server::send(HSteamNetConnection conn, std::string_view str) {
    netio->SendMessageToConnection(conn, str.data(), str.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void te::server::send_all(std::string_view str, HSteamNetConnection except) {
    for (auto [conn, client] : conn_clients) {
        if (conn != except) {
            send(conn, str);
        }
    }
}

std::vector<std::pair<te::client_handle, te::message_ptr>> te::server::recv() {
    std::vector<std::pair<te::client_handle, te::message_ptr>> received;
    ISteamNetworkingMessage* incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnPollGroup(poll_group, &incoming, 1);
    while (count_received == 1) {
        auto client_it = conn_clients.find(incoming->m_conn);
        if (client_it != conn_clients.end()) {
            received.emplace_back(std::piecewise_construct, std::forward_as_tuple(client_it->second), std::forward_as_tuple(incoming));
        }
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    } else {
        return received;
    }
}

#include <cereal/archives/json.hpp>
#include <sstream>
void te::server::poll() {
    netio->RunCallbacks(this);
    std::stringstream msg;
    {
        cereal::JSONOutputArchive output { msg };
        auto interesting = model.entities.view<te::named, te::site, te::footprint, te::render_mesh>();
        model.entities.snapshot().component<te::named, te::site, te::footprint, te::render_mesh>(output, interesting.begin(), interesting.end());
    }
    send_all(msg.str());
}

void te::server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    char temp[1024];
            
    // What's the state of the connection?
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
            auto client_it = conn_clients.find(info->m_hConn);
            assert(client_it != conn_clients.end());

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
            conn_clients.erase(client_it);
        } else {
            assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
        }
        // Do not linger because the connection is already closed on the other end
        netio->CloseConnection(info->m_hConn, 0, nullptr, false);
        break;
    }
    case k_ESteamNetworkingConnectionState_Connecting: {
        assert(conn_clients.find(info->m_hConn) == conn_clients.end()); // This must be a new connection
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

        conn_clients.emplace(info->m_hConn, client_handle{0, nick});
        netio->SetConnectionName(info->m_hConn, nick.c_str());
        break;
    }
    case k_ESteamNetworkingConnectionState_Connected:
        break;

    default:
        break;
    }
}
