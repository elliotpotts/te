#ifndef TE_CAMERA_HPP_INCLUDED
#define TE_CAMERA_HPP_INCLUDED

#include <glm/glm.hpp>
#include <complex>

namespace te {
    struct ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct camera {
        glm::vec3 focus;
        double altitude;
        std::complex<double> radius;
        float zoom_factor;
        float aspect_ratio;
        bool use_ortho = true;

        glm::vec3 eye() const;
        glm::vec3 forward() const;
        ray cast(glm::vec3 ndc) const;
        void zoom(float delta);
        glm::mat4 view() const;
        glm::mat4 projection() const;
    };
}

#endif // TE_CAMERA_HPP_INCLUDED
