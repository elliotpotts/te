#include <te/camera.hpp>
#include <glm/gtc/matrix_transform.hpp>

void te::camera::zoom(float delta) {
    zoom_factor = glm::clamp(zoom_factor + delta, 2.0f, 14.0f);
}

glm::mat4 te::camera::view() const {
    return glm::lookAt (
        focus + (offset * zoom_factor),
        focus,
        glm::vec3{0.0f, 0.0f, 1.0f}
    );
}

glm::mat4 te::camera::projection() const {
    if (use_ortho) {
        return glm::ortho(-zoom_factor, zoom_factor, -zoom_factor, zoom_factor, -1000.0f, 1000.0f);
    } else {
        return glm::perspective(glm::half_pi<float>(), 1024.0f/768.0f, 0.1f, 1000.0f);
    }
}
