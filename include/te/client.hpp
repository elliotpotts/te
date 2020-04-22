#ifndef TE_CLIENT_HPP_INCLUDED
#define TE_CLIENT_HPP_INCLUDED

#include <te/net.hpp>
#include <te/sim.hpp>
#include <span>
#include <string_view>

#include <sstream>

namespace te {
    class client {
    protected:
        virtual void send(std::span<const std::byte>) = 0;
    public:
        virtual ~client();
        virtual void poll() = 0;
        virtual std::optional<unsigned> family() = 0;
        
        template<typename T>
        void send(const T& msg) {
            std::string as_str = serialize_msg(msg);
            std::span<const std::byte> as_byte_span {reinterpret_cast<const std::byte*>(std::to_address(as_str.begin())), as_str.size()};
            send(as_byte_span);
        }
    };
    
    class net_client : private ISteamNetworkingSocketsCallbacks, public client {
        ISteamNetworkingSockets* netio;
        te::sim& model;
        HSteamNetConnection conn;
        std::optional<unsigned> my_family;
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
    public:
        virtual void send(std::span<const std::byte>) override;
    public:
        net_client(const SteamNetworkingIPAddr &serverAddr, te::sim& model);
        net_client(HSteamNetConnection conn, te::sim& model);
        virtual ~net_client();
        void handle(te::hello);
        void handle(te::full_update);
        void handle(te::msg_type);
        virtual void poll() override;
        virtual std::optional<unsigned> family() override;
    };
}

#endif
