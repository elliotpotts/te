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
            std::stringstream buffer;
            buffer.put(static_cast<unsigned char>(T::type));
            {
                cereal::BinaryOutputArchive output { buffer };
                output(msg);
            }
            std::string as_str = buffer.str();
            std::span<const char> as_char_span {as_str.cbegin(), as_str.cend()};
            std::span<const std::byte> as_byte_span {reinterpret_cast<const std::byte*>(as_char_span.data()), as_char_span.size_bytes()};
            send(as_byte_span);
        }
    };
    
    class net_client : private ISteamNetworkingSocketsCallbacks, public client {
        ISteamNetworkingSockets* netio;
        te::sim& model;
        HSteamNetConnection conn;
        virtual void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
    public:
        virtual void send(std::span<const std::byte>) override;
    public:
        net_client(const SteamNetworkingIPAddr &serverAddr, te::sim& model);
        net_client(HSteamNetConnection conn, te::sim& model);
        virtual ~net_client();
        virtual void poll() override;
        virtual std::optional<unsigned> family() override;
    };
}

#endif
