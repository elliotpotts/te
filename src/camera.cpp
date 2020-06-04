#include <te/camera.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

glm::vec3 te::camera::eye() const {
    return focus + glm::vec3{radius.real(), radius.imag(), altitude} * zoom_factor;
}

glm::vec3 te::camera::forward() const {
    return focus - eye();
}

te::ray te::camera::cast(glm::vec3 ray_ndc) const {
    if (use_ortho) {
        auto right = normalize(cross(forward(), glm::vec3{0.0f, 0.0f, 1.0f}));
        auto up = normalize(cross(forward(), right));
        return te::ray {
            .origin = eye() + (right * ray_ndc.x * zoom_factor * aspect_ratio) - (up * ray_ndc.y * zoom_factor),
            .direction = forward()
        };
    } else {
        const glm::vec4 ray_clip { ray_ndc.x, ray_ndc.y, -1.0f, 1.0f };
        glm::vec4 ray_eye = inverse(projection()) * ray_clip;
        ray_eye.z = -1.0f;
        ray_eye.w = 0.0f;
        const glm::vec4 ray_world_abnorm = inverse(view()) * ray_eye;
        const glm::vec3 ray_world = normalize (
            glm::vec3 {
                ray_world_abnorm.x,
                ray_world_abnorm.y,
                ray_world_abnorm.z
            }
        );
        return te::ray {
            .origin = eye(),
            .direction = ray_world
        };
    }
}

glm::mat4 te::camera::view() const {
    return glm::lookAt (
        eye(),
        focus,
        glm::vec3{0.0f, 0.0f, 1.0f}
    );
}

glm::mat4 te::camera::projection() const {
    if (use_ortho) {
        return glm::ortho(-zoom_factor * aspect_ratio, zoom_factor * aspect_ratio, -zoom_factor, zoom_factor, 0.1f, 1000.0f);
    } else {
        return glm::perspective(glm::half_pi<float>(), aspect_ratio, 0.1f, 1000.0f);
    }
}
