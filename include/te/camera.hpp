#ifndef TE_CAMERA_HPP_INCLUDED
#define TE_CAMERA_HPP_INCLUDED

#include <glm/glm.hpp>

namespace te {
    struct camera {
        glm::vec3 focus;
        glm::vec3 position;
        float zoom;
        glm::mat4 view() const;
        glm::mat4 projection() const;
    };
}

#endif // TE_CAMERA_HPP_INCLUDED
