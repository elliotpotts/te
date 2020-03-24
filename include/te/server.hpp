#ifndef TE_SERVER_HPP_INCLUDED
#define TE_SERVER_HPP_INCLUDED

#include <te/net.hpp>
#include <map>
#include <vector>

namespace te {
    struct client_handle {
        std::string nick;
    };
    
    struct server : private ISteamNetworkingSocketsCallbacks, public peer {
        ISteamNetworkingSockets* netio;
        HSteamListenSocket listen_sock;
        HSteamNetPollGroup poll_group;
        bool quit = false;

        std::map<HSteamNetConnection, client_handle> conn_clients;
        void send(HSteamNetConnection conn, std::string_view str);
        void send_all(std::string_view str, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
        void set_nick(HSteamNetConnection hConn, const char* new_nick);
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override;
    public:
        server();
        virtual ~server() = default;
        void listen(std::uint16_t port);
        std::vector<std::pair<te::client_handle, te::message_ptr>> poll();
        void run();
        void shutdown();
    };
}

#endif
