#include <te/window.hpp>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace {
    void glfw_error_callback(int error, const char* description) {
        spdlog::error("glfw3 error[{}]: {}", error, description);
    }
    void glfw_dispatch_framebuffer_size(GLFWwindow* w, int width, int height) {
        reinterpret_cast<te::window*>(glfwGetWindowUserPointer(w))->on_framebuffer_size(width, height);
    }
    void glfw_dispatch_key(GLFWwindow* w, int key, int scancode, int action, int mods) {
        reinterpret_cast<te::window*>(glfwGetWindowUserPointer(w))->on_key(key, scancode, action, mods);
    }
    void glfw_dispatch_cursor_pos(GLFWwindow* w, double xpos, double ypos) {
        reinterpret_cast<te::window*>(glfwGetWindowUserPointer(w))->on_cursor_move(xpos, ypos);
    }
    void glfw_dispatch_mouse_button(GLFWwindow* w, int button, int action, int mods) {
        reinterpret_cast<te::window*>(glfwGetWindowUserPointer(w))->on_mouse_button(button, action, mods);
    }
}

void te::window_deleter::operator()(GLFWwindow* win) const {
    glfwDestroyWindow(win);
}

te::window::window(glfw_context& glfw, window_hnd old_hnd) : glfw(glfw), hnd(std::move(old_hnd)) {
    glfwSetWindowUserPointer(hnd.get(), this);
    glfwSetInputMode(hnd.get(), GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(hnd.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetFramebufferSizeCallback(hnd.get(), glfw_dispatch_framebuffer_size);
    glfwSetKeyCallback(hnd.get(), glfw_dispatch_key);
    glfwSetCursorPosCallback(hnd.get(), glfw_dispatch_cursor_pos);
    glfwSetMouseButtonCallback(hnd.get(), glfw_dispatch_mouse_button);
}

int te::window::width() const {
    int width, height;
    glfwGetWindowSize(hnd.get(), &width, &height);
    return width;
}

int te::window::height() const {
    int width, height;
    glfwGetWindowSize(hnd.get(), &width, &height);
    return height;
}

void te::window::set_attribute(int attrib, int value) {
    glfwSetWindowAttrib(hnd.get(), attrib, value);
}

#include <FreeImage.h>
void te::window::set_cursor(te::unique_bitmap bmp) {
    auto pitch = FreeImage_GetPitch(bmp.get());
    GLFWimage cursor_img {
        .width = FreeImage_GetWidth(bmp.get()),
        .height = FreeImage_GetHeight(bmp.get()),
    };
    std::vector<unsigned char> pixels (pitch * cursor_img.height);
    cursor_img.pixels = pixels.data();
    FreeImage_ConvertToRawBits(cursor_img.pixels, bmp.get(), pitch, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);
    spdlog::debug("Cursor is {}x{}", cursor_img.width, cursor_img.height);
    GLFWcursor* cursor = glfwCreateCursor(&cursor_img, 0, 0);
    glfwSetCursor(hnd.get(), cursor);
}

int te::window::key(int k) const {
    return glfwGetKey(hnd.get(), k);
}

void te::window::close() {
    glfwSetWindowShouldClose(hnd.get(), true);
}

te::glfw_context::glfw_context() {
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialise glfw");
    } else {
        spdlog::info("Initialized glfw {}", glfwGetVersionString());
    }
    glfwSetErrorCallback(glfw_error_callback);
}

te::window te::glfw_context::make_window(int width, int height, const char* title, bool fullscreen = true) {
    //glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    auto window = window_hnd{glfwCreateWindow(width, height, title, fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr)};
    if (!window) {
        window.reset();
        throw std::runtime_error("Could not create opengl window");
    }
    glfwMakeContextCurrent(window.get());
    spdlog::info("Created window with following attributes: ");
    switch(glfwGetWindowAttrib(window.get(), GLFW_CLIENT_API)) {
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
    switch(glfwGetWindowAttrib(window.get(), GLFW_CONTEXT_CREATION_API)) {
    case GLFW_NATIVE_CONTEXT_API:
        spdlog::info(" |   GLFW_CONTEXT_CREATION_API: GLFW_NATIVE_CONTEXT_API");
        break;
    case GLFW_EGL_CONTEXT_API:
        spdlog::info(" |   GLFW_CONTEXT_CREATION_API: GLFW_EGL_CONTEXT_API");
        break;
    }
    spdlog::info(" |  GLFW_CONTEXT_VERSION_MAJOR: {}", glfwGetWindowAttrib(window.get(), GLFW_CONTEXT_VERSION_MAJOR));
    spdlog::info(" |  GLFW_CONTEXT_VERSION_MINOR: {}", glfwGetWindowAttrib(window.get(), GLFW_CONTEXT_VERSION_MINOR));
    spdlog::info(" |       GLFW_CONTEXT_REVISION: {}", glfwGetWindowAttrib(window.get(), GLFW_CONTEXT_REVISION));
    switch(glfwGetWindowAttrib(window.get(), GLFW_OPENGL_FORWARD_COMPAT)) {
    case GLFW_TRUE:
        spdlog::info(" |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_TRUE");
        break;
    case GLFW_FALSE:
        spdlog::info(" |  GLFW_OPENGL_FORWARD_COMPAT: GLFW_FALSE");
        break;
    }

    switch(glfwGetWindowAttrib(window.get(), GLFW_OPENGL_DEBUG_CONTEXT)) {
    case GLFW_TRUE:
        spdlog::info(" |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_TRUE");
        break;
    case GLFW_FALSE:
        spdlog::info(" |   GLFW_OPENGL_DEBUG_CONTEXT: GLFW_FALSE");
        break;
    }
    switch(glfwGetWindowAttrib(window.get(), GLFW_OPENGL_PROFILE)) {
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
    switch(glfwGetWindowAttrib(window.get(), GLFW_CONTEXT_ROBUSTNESS)) {
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
    return te::window{*this, std::move(window)};
}

te::glfw_context::~glfw_context() {
    glfwTerminate();
}
