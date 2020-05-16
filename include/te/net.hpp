#ifndef TE_NET_HPP_INCLUDED
#define TE_NET_HPP_INCLUDED

#include <memory>
#include <string>
#include <optional>
#include <tuple>
#include <variant>
#include <span>
#include <type_traits>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/variant.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <entt/entt.hpp>
#include <te/sim.hpp>
#include <te/components.hpp>

namespace glm {
    template<typename Ar>
    void serialize(Ar& ar, vec2& vec){
        ar(vec.x, vec.y);
    }
}

namespace te {
    struct hello {
        unsigned family;
        std::string nick;

        template<typename Ar>
        void serialize(Ar& ar) {
            ar(family, nick);
        }
    };

    struct chat {
        std::string from;
        std::string content;
        template<typename Ar>
        void serialize(Ar& ar) {
            ar(from, content);
        }
    };

    struct entity_create {
        entt::entity name;
        template<typename Ar>
        void serialize(Ar& ar) {
            ar(name);
        }
    };

    struct entity_delete {
        entt::entity name;
        template<typename Ar>
        void serialize(Ar& ar) {
            ar(name);
        }
    };

    //TODO: expand
    using cmpnt = std::variant<footprint, site, render_mesh>;
    struct component_replace {
        cmpnt component;
        template<typename Ar>
        void serialize(Ar& ar) {
            ar(component);
        }
    };

    using msg = std::variant<hello, chat, entity_create, entity_delete, component_replace>;

    template<typename T>
    std::string serialized(const T& msg) {
        std::stringstream stream {std::ios::out | std::ios::binary};
        {
            cereal::BinaryOutputArchive output { stream };
            output(msg);
        }
        return stream.str();
    }

    template<typename T>
    T deserialized(std::span<const char> buffer) {
        std::stringstream stream {std::ios::in | std::ios::out | std::ios::binary};
        auto stream_writer = std::ostreambuf_iterator { stream.rdbuf() };
        std::copy(buffer.begin(), buffer.end(), stream_writer);
        T deserialized;
        {
            cereal::BinaryInputArchive input { stream };
            input(deserialized);
        }
        return deserialized;
    }

    const int port = 7628;

    struct message_deleter {
        void operator()(ISteamNetworkingMessage* msg) const;
    };

    using message_ptr = std::unique_ptr<ISteamNetworkingMessage, message_deleter>;
}

#endif
