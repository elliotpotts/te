#ifndef TE_SERVER_HPP_INCLUDED
#define TE_SERVER_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <unordered_map>
#include <span>
#include <optional>
#include <memory>
#include <sstream>

namespace te {
    struct client;
    struct server : private ISteamNetworkingSocketsCallbacks {
        struct player {
            unsigned family;
            std::string nick;
        };

        ISteamNetworkingSockets* netio;
        HSteamListenSocket listen_sock;
        HSteamNetPollGroup poll_group;
        std::unordered_map<HSteamNetConnection, std::optional<player>> net_clients;

        void recv();
        void handle(HSteamNetConnection, te::hello);
        void handle(HSteamNetConnection, te::chat);
        void handle(HSteamNetConnection, te::entity_create);
        void handle(HSteamNetConnection, te::entity_delete);
        void handle(HSteamNetConnection, te::component_replace);
        void handle(HSteamNetConnection, te::build);
        sim model;
        bool started = false;
        void tick(double dt);

        void listen(std::uint16_t port);

        void send_bytes(HSteamNetConnection conn, std::span<const char> buffer);
        void send_bytes_all(std::span<const char> buffer, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
        void send(HSteamNetConnection conn, const te::msg& msg);
        void send_all(const te::msg& msg, HSteamNetConnection except = k_HSteamNetConnection_Invalid);

        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override;
    public:
        server(ISteamNetworkingSockets* netio, std::uint16_t port);
        client make_local(te::sim& model);
        virtual ~server();
        void poll(double dt);
        void run();
        void shutdown();
    };
}

#endif
