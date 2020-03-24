#ifndef TE_CLIENT_HPP_INCLUDED
#define TE_CLIENT_HPP_INCLUDED

#include <te/net.hpp>
#include <vector>

namespace te {
    class client : private ISteamNetworkingSocketsCallbacks, public peer {
        ISteamNetworkingSockets* netio;
        HSteamNetConnection conn;
        bool quit = false;

        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
        
    public:
        client(const SteamNetworkingIPAddr &serverAddr);
        virtual ~client() = default;
        std::vector<message_ptr> recv();
        void send(std::vector<char> buffer);
    };
}

#endif
