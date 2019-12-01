#include <random>
#include <te/window.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/colour_picker.hpp>
#include <glad/glad.h>
#include <chrono>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <complex>
#include <entt/entt.hpp>

template<typename T = float, typename Clock = std::chrono::high_resolution_clock>
struct phasor {
    const Clock::time_point birth;
    const T A;
    const T omega;
    const T phi;
    phasor(Clock::time_point birth, T A, T omega, T phi) : birth(birth), A(A), omega(omega), phi(phi) {
    }
    phasor(T A, T omega, T phi) : birth(Clock::now()), A(A), omega(omega), phi(phi) {
    }
    template<typename dt = std::chrono::milliseconds>
    std::complex<T> operator()(Clock::time_point t = Clock::now()) {
        return std::polar(A, omega * std::chrono::duration_cast<dt>(t - birth).count() * (static_cast<T>(dt::period::num) / static_cast<T>(dt::period::den)) + phi);
    }
};

namespace {
    glm::quat rotation_between(const glm::vec3& from, const glm::vec3& to) {
        // solution from https://stackoverflow.com/a/1171995/303662
        using namespace glm;
        const vec3 xyz = cross(from, to);
        const float w = std::sqrt(length2(from) * length2(to)) + dot(from, to);
        return normalize(quat{w, xyz});
    }
    glm::quat rotation_between_units(const glm::vec3& from, const glm::vec3& to) {
        using namespace glm;
        const vec3 xyz = cross(from, to);
        const float w = 1.0 + dot(from, to);
        return normalize(quat{w, xyz});
    }
}

struct client {
    std::random_device seed_device;
    std::mt19937 rengine;
    te::glfw_context glfw;
    te::window win;
    te::camera cam;
    te::terrain_renderer terrain_renderer;
    te::mesh_renderer mesh_renderer;
    te::colour_picker colour_picker;
    te::mesh fish_mesh;
    entt::registry entities;

    client() :
        win {glfw.make_window(1024, 768, "Hello, World!")},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-0.7f, -0.7f, 1.0f},
            8.0f
        },
        terrain_renderer(win.gl, rengine, 80, 80),
        mesh_renderer(win.gl),
        colour_picker(win),
        fish_mesh(te::load_mesh(win.gl, "house.glb", te::gl::common_attribute_locations))
        {
        win.on_key.connect([&](int key, int scancode, int action, int mods){ on_key(key, scancode, action, mods); });
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    void on_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    glm::mat4 model(float factor) {
        using namespace glm;
        mat4 resize {1.0f};
        // gltf convention says y+ is up, so rotate such that z+ is up
        quat rotate = rotation_between_units(vec3{0.0f, 1.0f, 0.0f}, vec3{0.0f, 0.0f, 1.0f});
        mat4 move = translate(mat4{1.0f}, vec3{-0.5f, -0.5f, 0.0f});
        return move * mat4_cast(rotate) * resize;
    }

    void draw() {
        colour_picker.colour_fbuffer.bind();
        colour_picker.draw(fish_mesh, model(1.0f),  0xff00ffff, cam);
        colour_picker.draw(fish_mesh, model(-1.0f), 0xffff00ff, cam);
        glFlush();
        glFinish();
        double mouse_x; double mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
        int under_mouse_x = static_cast<int>(mouse_x);
        int under_mouse_y = win.height - mouse_y;
        std::uint32_t id_under_cursor;
        win.gl.toggle_perf_warnings(false);
        glReadPixels(under_mouse_x, under_mouse_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &id_under_cursor);
        glClear(GL_COLOR_BUFFER_BIT);
        win.gl.toggle_perf_warnings(true);
        if (id_under_cursor == 0xff00ffff) {
            spdlog::debug("Fish 1 under cursor");
        } else if (id_under_cursor == 0xffff00ff) {
            spdlog::debug("Fish 2 under cursor");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        terrain_renderer.render(cam);
        mesh_renderer.draw(fish_mesh, model(1.0f), cam);
        mesh_renderer.draw(fish_mesh, model(-1.0f), cam);
    }

    void tick() {
        if (win.key(GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            win.close();
            return;
        };

        glm::vec3 forward = -cam.offset;
        forward.z = 0.0f;
        forward = glm::normalize(forward);
        glm::vec3 left = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        if (win.key(GLFW_KEY_W) == GLFW_PRESS) cam.focus += 0.1f * forward;
        if (win.key(GLFW_KEY_A) == GLFW_PRESS) cam.focus += 0.1f * left;
        if (win.key(GLFW_KEY_S) == GLFW_PRESS) cam.focus -= 0.1f * forward;
        if (win.key(GLFW_KEY_D) == GLFW_PRESS) cam.focus -= 0.1f * left;
        if (win.key(GLFW_KEY_H) == GLFW_PRESS) cam.zoom(0.15f);
        if (win.key(GLFW_KEY_J) == GLFW_PRESS) cam.zoom(-0.15f);
        cam.use_ortho = win.key(GLFW_KEY_SPACE) != GLFW_PRESS;
    }

    void run() {
        auto then = std::chrono::high_resolution_clock::now();
        int frames = 0;
        while (!glfwWindowShouldClose(win.hnd.get())) {
            tick();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            draw();
            glfwSwapBuffers(win.hnd.get());
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
    }
};

int main(void) {
    spdlog::set_level(spdlog::level::debug);

    client game;
    game.run();
    return 0;
}
