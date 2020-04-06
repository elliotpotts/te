#ifndef TE_NET_HPP_INCLUDED
#define TE_NET_HPP_INCLUDED

#include <memory>
#include <string>
#include <optional>
#include <tuple>
#include <span>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cereal/archives/binary.hpp>
#include <glm/vec2.hpp>

namespace glm {
    template<typename Ar>
    void serialize(Ar& ar, vec2& vec){
        ar(vec.x, vec.y);
    }
}

namespace te {
    const int port = 5060;
        
    struct message_deleter {
        void operator()(ISteamNetworkingMessage* msg) const;
    };
    
    using message_ptr = std::unique_ptr<ISteamNetworkingMessage, message_deleter>;

    template<typename T>
    T deserialize(std::span<const char> buffer) {
        std::stringstream strmsg;
        auto strmsg_writer = std::ostreambuf_iterator { strmsg.rdbuf() };
        std::copy(buffer.begin(), buffer.end(), strmsg_writer);
        T deserialized;
        {
            cereal::BinaryInputArchive input { strmsg };
            input(deserialized);
        }
        return deserialized;
    }
        
    enum class msg_type : unsigned char {
        hello,
        okay,
        ignored,
        fatal,
        full_update,
        chat
    };

    struct hello {
        static constinit const auto type = msg_type::hello;
        unsigned family;
        std::string nick;
        
        template<typename Ar>
        void serialize(Ar& ar) {
            ar(family, nick);
        }
    };

    struct okay {
        static constinit const auto type = msg_type::okay;
    };

    template<typename F>
    auto deserialize(std::span<const char> buffer, F&& f) {
        auto data = buffer.begin();
        const auto type = static_cast<msg_type>(*data++);
        std::span<const char> msg {data, buffer.end()};
        switch (type) {
        case msg_type::hello: return std::invoke(f, deserialize<hello>(msg));
        case msg_type::okay:
        case msg_type::ignored:
        case msg_type::fatal:
        case msg_type::full_update:
        case msg_type::chat:
            break;
        default:
            f(type);
        }
    }
}

#endif
