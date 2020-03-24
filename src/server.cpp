#include <te/server.hpp>
#include <spdlog/spdlog.h>

te::server::server() : netio{SteamNetworkingSockets()} {
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

std::vector<std::pair<te::client_handle, te::message_ptr>> te::server::poll() {
    netio->RunCallbacks(this);

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

void te::server::set_nick(HSteamNetConnection hConn, const char* new_nick) {
    conn_clients[hConn].nick = new_nick;
    // Set the connection name, too, which is useful for debugging
    netio->SetConnectionName(hConn, new_nick);
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
        // Ignore if they were not previously connected.  (If they disconnected
        // before we accepted the connection.)
        if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
            // Locate the client.  Note that it should have been found, because this
            // is the only codepath where we remove clients (except on shutdown),
            // and connection change callbacks are dispatched in queue order.
            auto client_it = conn_clients.find(info->m_hConn);
            assert(client_it != conn_clients.end());

            // Select appropriate log messages
            const char *pszDebugLogAction;
            if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                pszDebugLogAction = "problem detected locally";
                sprintf(temp, "Alas, %s hath fallen into shadow.  (%s)", client_it->second.nick.c_str(), info->m_info.m_szEndDebug);
            } else {
                // Note that here we could check the reason code to see if
                // it was a "usual" connection or an "unusual" one.
                pszDebugLogAction = "closed by peer";
                sprintf(temp, "%s hath departed", client_it->second.nick.c_str());
            }

            // Spew something to our own log.  Note that because we put their nick
            // as the connection description, it will show up, along with their
            // transport-specific data (e.g. their IP address)
            spdlog::debug (
                "Connection {} {}, reason {}: {}",
                info->m_info.m_szConnectionDescription,
                pszDebugLogAction,
                info->m_info.m_eEndReason,
                info->m_info.m_szEndDebug
            );

            conn_clients.erase(client_it);

            // Send a message so everybody else knows what happened
            send_all(temp);
        } else {
            assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
        }

        // Clean up the connection.  This is important!
        // The connection is "closed" in the network sense, but
        // it has not been destroyed.  We must close it on our end, too
        // to finish up.  The reason information do not matter in this case,
        // and we cannot linger because it's already closed on the other end,
        // so we just pass 0's.
        netio->CloseConnection(info->m_hConn, 0, nullptr, false);
        break;
    }
    case k_ESteamNetworkingConnectionState_Connecting: {
        // This must be a new connection
        assert(conn_clients.find(info->m_hConn) == conn_clients.end());

        spdlog::info("Connection request from {}", info->m_info.m_szConnectionDescription);

        // A client is attempting to connect
        // Try to accept the connection.
        if (netio->AcceptConnection(info->m_hConn) != k_EResultOK) {
            // This could fail.  If the remote host tried to connect, but then
            // disconnected, the connection may already be half closed.  Just
            // destroy whatever we have on our side.
            netio->CloseConnection(info->m_hConn, 0, nullptr, false);
            spdlog::info("Can't accept connection. (It was already closed?)");
            break;
        }

        // Assign the poll group
        if (!netio->SetConnectionPollGroup(info->m_hConn, poll_group)) {
            netio->CloseConnection( info->m_hConn, 0, nullptr, false );
            spdlog::info("Failed to set poll group?");
            break;
        }

        // Generate a random nick.  A random temporary nick
        // is really dumb and not how you would write a real chat server.
        // You would want them to have some sort of signon message,
        // and you would keep their client in a state of limbo (connected,
        // but not logged on) until them.  I'm trying to keep this example
        // code really simple.
        char nick[64];
        sprintf(nick, "BraveWarrior%d", 10000 + (rand() % 100000));

        // Send them a welcome message
        sprintf(temp, "Welcome, stranger.  Thou art known to us for now as '%s'; upon thine command '/nick' we shall know thee otherwise.", nick); 
        send(info->m_hConn, temp); 

        // Also send them a list of everybody who is already connected
        if (conn_clients.empty()) {
            send(info->m_hConn, "Thou art utterly alone."); 
        } else {
            sprintf(temp, "%d companions greet you:", (int)conn_clients.size());
            for (auto &c: conn_clients) {
                send(info->m_hConn, c.second.nick.c_str());
            }
        }

        // Let everybody else know who they are for now
        sprintf(temp, "Hark!  A stranger hath joined this merry host.  For now we shall call them '%s'", nick); 
        send_all(temp, info->m_hConn); 

        // Add them to the client list, using std::map wacky syntax
        conn_clients[info->m_hConn];
        set_nick(info->m_hConn, nick);
        break;
    }
    case k_ESteamNetworkingConnectionState_Connected:
        // We will get a callback immediately after accepting the connection.
        // Since we are the server, we can ignore this, it's not news to us.
        break;

    default:
        break;
    }
}
