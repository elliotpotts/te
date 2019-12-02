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
#include <unordered_set>

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

struct position {
    glm::mat4 model;
};

struct render_mesh {
    te::mesh* mesh;
};

struct pickable_mesh {
    te::mesh* mesh;
    std::uint32_t id;
};

struct client {
    std::random_device seed_device;
    std::mt19937 rengine;
    te::glfw_context glfw;
    te::window win;
    te::camera cam;
    te::terrain_renderer terrain_renderer;
    te::mesh_renderer mesh_renderer;
    te::colour_picker colour_picker;
    te::mesh house_mesh;
    entt::registry entities;
    std::optional<entt::registry::entity_type> entity_under_cursor;

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
        house_mesh(te::load_mesh(win.gl, "house.glb", te::gl::common_attribute_locations))
        {
        win.on_key.connect([&](int key, int scancode, int action, int mods){ on_key(key, scancode, action, mods); });
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        for (int i = 0; i < 200; i++) append_house();
    }

    void on_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    glm::mat4 make_house_model_matrix() {
        static std::uniform_int_distribution tile_pos_select {-40, 40};
        //static std::unordered_set<glm::vec2> taken_positions;
        static std::uniform_int_distribution rotation_select {0, 3};
        using namespace glm;
        mat4 resize {1.0f};
        // gltf convention says y+ is up, so rotate such that z+ is up
        quat rotate_zup = rotation_between_units(vec3{0.0f, 1.0f, 0.0f}, vec3{0.0f, 0.0f, 1.0f});
        quat rotate_variation = normalize( quat (
            glm::half_pi<float>() * static_cast<float>(rotation_select(rengine)),
            glm::vec3 {0.0f, 0.0f, 1.0f}
        ));
        mat4 move = translate(
            mat4 {1.0f},
            vec3 {-0.5f + static_cast<float>(tile_pos_select(rengine)),
                  -0.5f + static_cast<float>(tile_pos_select(rengine)),
                   0.0f}
        );
        return move * mat4_cast(rotate_variation * rotate_zup) * resize;
    }

    void append_house() {
        static std::uint32_t house_id = 1;
        auto house = entities.create();
        entities.assign<position>(house, make_house_model_matrix());
        entities.assign<render_mesh>(house, &house_mesh);
        entities.assign<pickable_mesh>(house, &house_mesh, house_id++);
    }

    void draw() {
        colour_picker.colour_fbuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        entities.view<position, pickable_mesh>().each(
            [&](auto& pos, auto& pick) {
                colour_picker.draw(*pick.mesh, pos.model, pick.id, cam);
            }
        );
        glFlush();
        glFinish();
        double mouse_x; double mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
        int under_mouse_x = static_cast<int>(mouse_x);
        int under_mouse_y = win.height - mouse_y;
        std::uint32_t id_under_cursor;
        win.gl.toggle_perf_warnings(false);
        glReadPixels(under_mouse_x, under_mouse_y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &id_under_cursor);
        win.gl.toggle_perf_warnings(true);
        auto pickable_entities = entities.view<pickable_mesh>();
        auto entity_it = std::find_if (
            pickable_entities.begin(),
            pickable_entities.end(),
            [&,id_under_cursor](auto entity) {
                return (pickable_entities.get<pickable_mesh>(entity)).id == id_under_cursor;
            }
        );
        if (entity_it != pickable_entities.end()) {
            if (!entity_under_cursor || *entity_it != *entity_under_cursor) {
                spdlog::debug("Under cursor: {}", id_under_cursor);
                entity_under_cursor = *entity_it;
            }
        } else {
            entity_under_cursor.reset();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        terrain_renderer.render(cam);
        entities.view<position, render_mesh>().each (
            [&](auto& pos, auto& rmesh) {
                mesh_renderer.draw(*rmesh.mesh, pos.model, cam);
            }
        );
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
