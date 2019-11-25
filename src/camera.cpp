#include <te/camera.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 te::camera::view() const {
    return glm::lookAt (
        focus + offset,
        focus,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );
}

glm::mat4 te::camera::projection() const {
    //return glm::perspective(glm::half_pi<float>(), 1024.0f/768.0f, 0.1f, 100.0f);
    return glm::ortho(-5.0f - zoom, 5.0f + zoom, -5.0f - zoom, 5.0f + zoom, 0.00001f, 100.0f);
}
