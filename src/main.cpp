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
#define  BOOST_LOG_DYN_LINK
#include <boost/log/trivial.hpp>
#include <te/opengl.hpp>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>

std::random_device seed_device;
std::mt19937 rengine;

auto start_time = std::chrono::high_resolution_clock::now();

class game {
    GLFWwindow* w;
    te::camera cam;
    te::terrain_renderer terrain;

public:
    game(GLFWwindow* w):
        w {w},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-48.0f, -48.0f, 48.0f}
        },
        terrain(rengine, 80, 80)
        {
    }

    void handle_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.position = glm::rotate(cam.position, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.position = glm::rotate(cam.position, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    float zoom = 1.0f;

    void operator()() {
        if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(w, true); };
        glClear(GL_COLOR_BUFFER_BIT);

        std::chrono::duration<float> secs = start_time - std::chrono::high_resolution_clock::now();

        glm::vec3 forward = -cam.position;
        forward.z = 0.0f;
        forward = glm::normalize(forward);
        
        glm::vec3 right = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.focus += 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.focus -= 0.1f * right;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.focus -= 0.1f * forward;
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.focus += 0.1f * right;
	if (glfwGetKey(w, GLFW_KEY_H) == GLFW_PRESS) cam.zoom += 0.05f;
	if (glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS) cam.zoom -= 0.05f;

        terrain.render(cam);
    }
};

void glfw_error_callback(int error, const char* description) {
    BOOST_LOG_TRIVIAL(error) << "glfw3 Error[" << error << "]: " << description;
    std::abort();
}

void opengl_error_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    BOOST_LOG_TRIVIAL(error) << "opengl error: " << message;
}

void glfw_dispatch_key(GLFWwindow* w, int key, int scancode, int action, int mods) {
    reinterpret_cast<game*>(glfwGetWindowUserPointer(w))
    ->handle_key(key, scancode, action, mods);
}

int const win_w = 1024;
int const win_h = 768;
int main(void) {
    if (!glfwInit()) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not initialise glfw";
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Initialized glfw " << glfwGetVersionString();
    }

    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_RESIZABLE , false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "Hello World", nullptr, nullptr);
    if (!window) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not create opengl window";
        glfwTerminate();
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Created window with following attributes: ";
        switch(glfwGetWindowAttrib(window, GLFW_CLIENT_API)) {
            case GLFW_OPENGL_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_OPENGL_API";
                break;
            case GLFW_OPENGL_ES_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_OPENGL_ES_API";
                break;
            case GLFW_NO_API:
                BOOST_LOG_TRIVIAL(info) << " |             GLFW_CLIENT_API: GLFW_NO_API";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_CREATION_API)) {
            case GLFW_NATIVE_CONTEXT_API:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_CONTEXT_CREATION_API: GLFW_NATIVE_CONTEXT_API";
                break;
            case GLFW_EGL_CONTEXT_API:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_CONTEXT_CREATION_API: GLFW_EGL_CONTEXT_API";
                break;
        }
        BOOST_LOG_TRIVIAL(info) << " |  GLFW_CONTEXT_VERSION_MAJOR: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
        BOOST_LOG_TRIVIAL(info) << " |  GLFW_CONTEXT_VERSION_MINOR: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
        BOOST_LOG_TRIVIAL(info) << " |       GLFW_CONTEXT_REVISION: " << glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_FORWARD_COMPAT)) {
            case GLFW_TRUE:
                BOOST_LOG_TRIVIAL(info) << " |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_TRUE";
                break;
            case GLFW_FALSE:
                BOOST_LOG_TRIVIAL(info) << " |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_FALSE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_DEBUG_CONTEXT)) {
            case GLFW_TRUE:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_TRUE";
                break;
            case GLFW_FALSE:
                BOOST_LOG_TRIVIAL(info) << " |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_FALSE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE)) {
            case GLFW_OPENGL_CORE_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_CORE_PROFILE";
                break;
            case GLFW_OPENGL_COMPAT_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_COMPAT_PROFILE";
                break;
            case GLFW_OPENGL_ANY_PROFILE:
                BOOST_LOG_TRIVIAL(info) << " |         GLFW_OPENGL_PROFILE: GLFW_OPENGL_ANY_PROFILE";
                break;
        }
        switch(glfwGetWindowAttrib(window, GLFW_CONTEXT_ROBUSTNESS)) {
            case GLFW_LOSE_CONTEXT_ON_RESET:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_LOSE_CONTEXT_ON_RESET";
                break;
            case GLFW_NO_RESET_NOTIFICATION:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_RESET_NOTIFICATION";
                break;
            case GLFW_NO_ROBUSTNESS:
                BOOST_LOG_TRIVIAL(info) << " |     GLFW_CONTEXT_ROBUSTNESS: GLFW_NO_ROBUSTNESS";
                break;
        }
    }

    glfwMakeContextCurrent(window);
    BOOST_LOG_TRIVIAL(info) << "Context is now current.";
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        BOOST_LOG_TRIVIAL(fatal) << "Could not load opengl extensions";
        return -1;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Loaded opengl extensions";
    }
    glDebugMessageCallback(opengl_error_callback, nullptr);
    game g(window);
    glfwSetWindowUserPointer(window, &g);
    glfwSetKeyCallback(window, glfw_dispatch_key);
    while (!glfwWindowShouldClose(window)) {
        g();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
