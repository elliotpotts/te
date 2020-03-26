#ifndef TE_SERVER_HPP_INCLUDED
#define TE_SERVER_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <unordered_map>
#include <vector>

namespace te {
    struct client_handle {
        unsigned family;
        std::string nick;
    };
    
    struct server : private ISteamNetworkingSocketsCallbacks, public peer {
        ISteamNetworkingSockets* netio;
        HSteamListenSocket listen_sock;
        HSteamNetPollGroup poll_group;
        sim& model;

        std::unordered_map<HSteamNetConnection, client_handle> conn_clients;
        void send(HSteamNetConnection conn, std::string_view str);
        void send_all(std::string_view str, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
        std::vector<std::pair<te::client_handle, te::message_ptr>> recv();
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override;
    public:
        server(te::sim& model);
        virtual ~server();
        void listen(std::uint16_t port);
        virtual void poll() override;
        void run();
        void shutdown();
    };
}

#endif
