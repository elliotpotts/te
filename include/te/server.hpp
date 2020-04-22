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

        void recv(message_ptr incoming);
        void handle(HSteamNetConnection, te::hello);
        void handle(HSteamNetConnection, te::full_update);
        void handle(HSteamNetConnection, te::msg_type);

        sim& model;
        
        void listen(std::uint16_t port);
        
        void send_bytes(HSteamNetConnection conn, std::span<const char> buffer);
        void send_bytes_all(std::span<const char> buffer, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
        template<typename T>
        void send_msg(HSteamNetConnection conn, const T& msg) {
            std::string as_str = serialize_msg(msg);
            std::span<const char> as_char_span {as_str.cbegin(), as_str.cend()};
            send_bytes(conn, as_char_span);
        }
        template<typename T>
        void send_msg_all(const T& msg, HSteamNetConnection except = k_HSteamNetConnection_Invalid) {
            for (auto [conn, client] : net_clients) {
                if (conn != except) {
                    send_msg(conn, msg);
                }
            }
        }
        
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override;
    public:
        server(std::uint16_t port, te::sim& model);
        std::unique_ptr<client> make_local(te::sim& model);
        virtual ~server();
        void poll();
        void run();
        void shutdown();
    };
}

#endif
