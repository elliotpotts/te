#ifndef TE_UTIL_HPP_INCLUDED
#define TE_UTIL_HPP_INCLUDED

#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace te {
    std::string file_contents(std::string filename);
}

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
