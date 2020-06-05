#ifndef TE_WINDOW_HPP_INCLUDED
#define TE_WINDOW_HPP_INCLUDED

#include <GLFW/glfw3.h>
#include <memory>
#include <boost/signals2.hpp>
#include <te/gl.hpp>
#include <te/image.hpp>

namespace te {
    struct window_deleter {
        void operator()(GLFWwindow* w) const;
    };
    using window_hnd = std::unique_ptr<GLFWwindow, window_deleter>;
    struct glfw_context;

    struct window {
        glfw_context& glfw;
        window_hnd hnd;
        int width() const;
        int height() const;
        gl::context gl;
        boost::signals2::signal<void(int, int)> on_framebuffer_size;
        boost::signals2::signal<void(int, int, int, int)> on_key;
        boost::signals2::signal<void(double, double)> on_cursor_move;
        boost::signals2::signal<void(int, int, int)> on_mouse_button;
        window(glfw_context&, window_hnd);
        window(const window&&) = delete; //if implemented, glfwuserpointer must be updated
        int key(int) const;
        void set_attribute(int attrib, int value);
        void set_cursor(unique_bitmap bmp);
        void close();
    };

    struct glfw_context {
        explicit glfw_context();
        window make_window(int w, int h, const char* title, bool fullscreen);
        ~glfw_context();
    };
}

#endif
