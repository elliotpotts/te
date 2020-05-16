#include <te/server.hpp>
#include <te/app.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

te::server::server(ISteamNetworkingSockets* netio, std::uint16_t port) :
    netio { netio },
    //TODO: figure seed shit out
    model { 42 } {
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

void te::server::send(HSteamNetConnection conn, const te::msg& msg) {
    std::string as_str = serialized(msg);
    std::span<const char> as_char_span {as_str.cbegin(), as_str.cend()};
    send_bytes(conn, as_char_span);
}

void te::server::send_all(const te::msg& msg, HSteamNetConnection except) {
    for (auto [conn, client] : net_clients) {
        if (conn != except) {
            send(conn, msg);
        }
    }
}

void te::server::run() {
}

void te::server::handle(HSteamNetConnection conn, te::hello msg) {
    spdlog::debug("hello: family {}, {}", msg.family, msg.nick);
    netio->SetConnectionName(conn, msg.nick.c_str());
    auto player_it = net_clients.find(conn);
    if (!player_it->second) {
        player_it->second.emplace(msg.family, msg.nick);
    } else {
        spdlog::error("client <{}, {}> trying to hello twice", player_it->second->nick, player_it->second->family);
    }
}
void te::server::handle(HSteamNetConnection conn, te::chat msg) {
    send_all(msg);
}
void te::server::handle(HSteamNetConnection conn, te::entity_create) {
}
void te::server::handle(HSteamNetConnection conn, te::entity_delete) {
}
void te::server::handle(HSteamNetConnection conn, te::component_replace) {
}

te::client te::server::make_local(te::sim& model) {
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
    net_clients.emplace(server_end, std::nullopt);
    return te::client{netio, client_end, model};
}

void te::server::recv() {
    ISteamNetworkingMessage* incoming = nullptr;
    int count_received = netio->ReceiveMessagesOnPollGroup(poll_group, &incoming, 1);
    while (count_received == 1) {
        message_ptr received { incoming };
        std::span payload {
            static_cast<const char*>(received->m_pData),
            static_cast<std::size_t>(received->m_cbSize)
        };
        auto received_msg = te::deserialized<te::msg>(payload);
        std::visit([&](auto& m) {
            handle(received->m_conn, m);
        }, received_msg);
        count_received = netio->ReceiveMessagesOnPollGroup(poll_group, &incoming, 1);
    }
    if (count_received < 0) {
        throw std::runtime_error{"Error checking for messages"};
    }
}

void te::server::poll(double dt) {
    netio->RunCallbacks(this);
    recv();
    tick(dt);
}

void te::server::tick(double dt) {
    int players_hellod = std::count_if (
        net_clients.begin(),
        net_clients.end(),
        [](auto& pair) {
            auto& [conn, player] = pair;
            return player.has_value();
        }
    );

    if (players_hellod == 1 && !started) {
        started = true;
        spdlog::debug("We have {} players. Starting game with:", players_hellod);
        for (auto [conn, player] : net_clients) {
            if (player) {
                spdlog::debug("*  {} as family {}", player->nick, player->family);
                send(conn, te::hello{player->family, player->nick});
            } else {
                spdlog::debug("*  <spectator>");
            }
        }
    }

    if (started) {
        model.tick(dt);
    }
}

void te::server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
    if (info->m_hConn != listen_sock) {
        // this status change is probably intended for a client on the same host as the server
        spdlog::debug("hmmm...?");
        //return;
    }
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
        if (!netio->SetConnectionPollGroup(info->m_hConn, poll_group)) {
            netio->CloseConnection(info->m_hConn, 0, nullptr, false);
            spdlog::info("Failed to set poll group?");
            break;
        }
        net_clients.emplace(info->m_hConn, std::nullopt);
        break;
    }
    case k_ESteamNetworkingConnectionState_Connected:
        spdlog::debug("k_ESteamNetworkingConnectionState_Connected");
        break;

    default:
        break;
    }
}
