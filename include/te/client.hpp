#ifndef TE_CLIENT_HPP_INCLUDED
#define TE_CLIENT_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <vector>

namespace te {
    class client : private ISteamNetworkingSocketsCallbacks, public peer {
        ISteamNetworkingSockets* netio;
        HSteamNetConnection conn;
        bool quit = false;
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
        std::vector<message_ptr> recv();
        void send(std::vector<char>);

        te::sim& model;
    public:
        client(const SteamNetworkingIPAddr &serverAddr, te::sim& model);
        virtual ~client() = default;
        virtual void poll() override;
    };
}

#endif
