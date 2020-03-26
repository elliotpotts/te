#ifndef TE_CLIENT_HPP_INCLUDED
#define TE_CLIENT_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <vector>

namespace te {
    class client : private ISteamNetworkingSocketsCallbacks, public peer {
        ISteamNetworkingSockets* netio;
        te::sim& model;
        HSteamNetConnection conn;
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
        std::vector<message_ptr> recv();
        void send(std::vector<char>);
        
    public:
        client(const SteamNetworkingIPAddr &serverAddr, te::sim& model);
        virtual ~client();
        virtual void poll() override;
    };
}

#endif
