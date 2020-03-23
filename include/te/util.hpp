#ifndef TE_UTIL_HPP_INCLUDED
#define TE_UTIL_HPP_INCLUDED

#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace te {
    std::string file_contents(std::string filename);
}

template<typename T>
struct type_tag {
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>; // not needed as of C++20

template<class T> struct always_false : std::false_type {};

namespace std {
    template<>
    struct hash<glm::vec2> {
        std::size_t operator()(glm::vec2 xy) const;
    };
    template<>
    struct hash<glm::ivec2> {
        std::size_t operator()(glm::vec2 xy) const;
    };
}

#endif
