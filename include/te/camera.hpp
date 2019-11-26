#ifndef TE_CAMERA_HPP_INCLUDED
#define TE_CAMERA_HPP_INCLUDED

#include <glm/glm.hpp>

namespace te {
    struct camera {
        glm::vec3 focus;
        glm::vec3 offset;
        float zoom_factor;
        void zoom(float delta);
        glm::mat4 view() const;
        glm::mat4 projection() const;
        bool use_ortho = true;
    };
}

#endif // TE_CAMERA_HPP_INCLUDED
