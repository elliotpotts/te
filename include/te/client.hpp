#ifndef TE_CLIENT_HPP_INCLUDED
#define TE_CLIENT_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <span>
#include <boost/signals2.hpp>
#include <cstdint>

namespace te {
    class client : private ISteamNetworkingSocketsCallbacks {
        ISteamNetworkingSockets* netio;
        HSteamNetConnection conn;
    protected:
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override;

    public:
        std::int64_t send(std::span<const std::byte>);

        void handle(te::hello);
        void handle(te::chat);
        void handle(te::entity_create);
        void handle(te::entity_delete);
        void handle(te::component_replace);
        void handle(te::build);

        te::sim& model;
        std::optional<unsigned> my_family;
        std::optional<std::string> my_nick;

        static HSteamNetConnection connect_to_addr(ISteamNetworkingSockets* netio, const SteamNetworkingIPAddr &serverAddr);

    public:
        client(ISteamNetworkingSockets* netio, HSteamNetConnection conn, te::sim& model);
        client(ISteamNetworkingSockets* netio, const SteamNetworkingIPAddr &serverAddr, te::sim& model);
        client(client&& rhs);
        virtual ~client();
        void poll(double elapsed);
        std::int64_t send(te::msg&& m);
        std::optional<unsigned> family();
        std::optional<std::string> nick();
        boost::signals2::signal<void(te::chat)> on_chat;
    };
}

#endif
