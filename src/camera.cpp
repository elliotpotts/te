#include <te/camera.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 te::camera::view() const {
    return glm::lookAt (
        focus - position,
        focus,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );
}

glm::mat4 te::camera::projection() const {
    return glm::ortho(-5.0f - zoom, 5.0f + zoom, -5.0f - zoom, 5.0f + zoom, 0.00001f, 500.0f);
}
