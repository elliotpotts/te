#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <vector>
#include <tuple>
#include <random>
#include <spdlog/spdlog.h>
#include <te/util.hpp>
#include <te/opengl.hpp>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <fmt/core.h>


std::random_device seed_device;
std::mt19937 rengine;

class client {
    GLFWwindow* w;
    te::camera cam;
    te::terrain_renderer terrain;
    te::mesh_renderer meshr;
public:
    client(GLFWwindow* w):
        w {w},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-8.0f, -8.0f, 8.0f}
        },
        terrain(rengine, 80, 80),
        meshr("BoxTextured.glb") {
    }

    void handle_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    void handle_cursor_pos(double xpos, double ypos) {
    }

    void operator()() {
        if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(w, true); };
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 forward = -cam.offset;
        forward.z = 0.0f;
        forward = glm::normalize(forward);
        glm::vec3 left = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.focus += 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.focus += 0.1f * left;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.focus -= 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.focus -= 0.1f * left;
	if (glfwGetKey(w, GLFW_KEY_H) == GLFW_PRESS) cam.zoom += 0.05f;
	if (glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS) cam.zoom -= 0.05f;

        terrain.render(cam);
        meshr.draw(cam);
    }
};

void glfw_error_callback(int error, const char* description) {
    spdlog::error("glfw3 error[{}]: {}", error, description);
    std::abort();
}

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    spdlog::error("opengl error: {}", message);
}

void glfw_dispatch_key(GLFWwindow* w, int key, int scancode, int action, int mods) {
    reinterpret_cast<client*>(glfwGetWindowUserPointer(w))
        ->handle_key(key, scancode, action, mods);
}

void glfw_dispatch_cursor_pos(GLFWwindow* w, double xpos, double ypos) {
    reinterpret_cast<client*>(glfwGetWindowUserPointer(w))
        ->handle_cursor_pos(xpos, ypos);
}
struct glfw_library {
    explicit glfw_library();
    ~glfw_library();
};
glfw_library::glfw_library() {
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialise glfw");
    } else {
        spdlog::info("Initialized glfw {}", glfwGetVersionString());
    }
}
glfw_library::~glfw_library() {
    glfwTerminate();
}
#include <cstddef>
int const win_w = 1024;
int const win_h = 768;
int main(void) {
    glfw_library glfw;
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_RESIZABLE , false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "Hello World", nullptr, nullptr);
    if (!window) {
        spdlog::error("Could not create opengl window");
        glfwTerminate();
        return -1;
    } else {
        spdlog::info("Created window with following attributes: ");
        switch(glfwGetWindowAttrib(window, GLFW_CLIENT_API)) {
            case GLFW_OPENGL_API:
                spdlog::info(" |             GLFW_CLIENT_API: GLFW_OPENGL_API");
                break;
            case GLFW_OPENGL_ES_API:
                spdlog::info(" |             GLFW_CLIENT_API: GLFW_OPENGL_ES_API");
                break;
            case GLFW_NO_API:
                spdlog::info(" |             GLFW_CLIENT_API: GLFW_NO_API");
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_CREATION_API)) {
            case GLFW_NATIVE_CONTEXT_API:
                spdlog::info(" |   GLFW_CONTEXT_CREATION_API: GLFW_NATIVE_CONTEXT_API");
                break;
            case GLFW_EGL_CONTEXT_API:
                spdlog::info(" |   GLFW_CONTEXT_CREATION_API: GLFW_EGL_CONTEXT_API");
                break;
        }
        spdlog::info(" |  GLFW_CONTEXT_VERSION_MAJOR: {}", glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR));
        spdlog::info(" |  GLFW_CONTEXT_VERSION_MINOR: {}", glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR));
        spdlog::info(" |       GLFW_CONTEXT_REVISION: {}", glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION));
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_FORWARD_COMPAT)) {
            case GLFW_TRUE:
                spdlog::info(" |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_TRUE");
                break;
            case GLFW_FALSE:
                spdlog::info(" |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_FALSE");
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_DEBUG_CONTEXT)) {
            case GLFW_TRUE:
                spdlog::info(" |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_TRUE");
                break;
            case GLFW_FALSE:
                spdlog::info(" |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_FALSE");
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE)) {
            case GLFW_OPENGL_CORE_PROFILE:
                spdlog::info(" |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_CORE_PROFILE");
                break;
            case GLFW_OPENGL_COMPAT_PROFILE:
                spdlog::info(" |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_COMPAT_PROFILE");
                break;
            case GLFW_OPENGL_ANY_PROFILE:
                spdlog::info(" |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_ANY_PROFILE");
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_ROBUSTNESS)) {
            case GLFW_LOSE_CONTEXT_ON_RESET:
                spdlog::info(" |     GLFW_CONTEXT_ROBUSTNESS: GLFW_LOSE_CONTEXT_ON_RESET");
                break;
            case GLFW_NO_RESET_NOTIFICATION:
                spdlog::info(" |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_RESET_NOTIFICATION");
                break;
            case GLFW_NO_ROBUSTNESS:
                spdlog::info(" |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_ROBUSTNESS");
                break;
        }
    }

    glfwMakeContextCurrent(window);
    spdlog::info("Context is now current.");
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        spdlog::error("Could not load opengl extensions");
        return -1;
    } else {
        spdlog::info("Loaded opengl extensions");
    }
    glDebugMessageCallback(opengl_error_callback, nullptr);

    client g(window);
    glfwSetWindowUserPointer(window, &g);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetKeyCallback(window, glfw_dispatch_key);
    glfwSetCursorPosCallback(window, glfw_dispatch_cursor_pos);

    auto then = std::chrono::high_resolution_clock::now();
    int frames = 0;
    
    while (!glfwWindowShouldClose(window)) {
        g();
        glfwSwapBuffers(window);
        glfwPollEvents();
        frames++;
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> secs = now - then;
        if (secs.count() > 1.0) {
            fmt::print("fps: {}\n", frames / secs.count());
            frames = 0;
            then = std::chrono::high_resolution_clock::now();
        }
    }
    return 0;
}
