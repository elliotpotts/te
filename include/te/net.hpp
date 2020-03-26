#ifndef TE_NET_HPP_INCLUDED
#define TE_NET_HPP_INCLUDED

#include <memory>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

namespace te {
    const int port = 5060;

    void network_debug_output(ESteamNetworkingSocketsDebugOutputType type, const char* message);
    
    struct message_deleter {
        void operator()(ISteamNetworkingMessage* msg) const;
    };
    using message_ptr = std::unique_ptr<ISteamNetworkingMessage, message_deleter>;

    struct peer {
        virtual ~peer();
        virtual void poll() = 0;
    };
}

#endif
