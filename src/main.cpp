#include <random>
#include <te/window.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/colour_picker.hpp>
#include <glad/glad.h>
#include <chrono>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <complex>

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

struct client {
    std::random_device seed_device;
    std::mt19937 rengine;
    te::glfw_context glfw;
    te::window win;
    te::camera cam;
    te::terrain_renderer terrain;
    te::mesh_renderer meshr;
    te::colour_picker colpickr;
    te::mesh fish_mesh;
    phasor<> fish;

    client() :
        win {glfw.make_window(1024, 768, "Hello, World!")},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-0.7f, -0.7f, 1.0f},
            8.0f
        },
        terrain(win.gl, rengine, 80, 80),
        meshr(win.gl),
        colpickr(win),
        fish_mesh(te::load_mesh(win.gl, "BarramundiFish.glb", te::gl::common_attribute_locations)),
        fish {5.0f, glm::half_pi<float>() / 100.0f, 0.0f}
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
        glm::mat4 orient = glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});
        glm::mat4 turn = glm::rotate(glm::mat4{1.0f}, std::arg(fish() * factor), glm::vec3{0.0f, 0.0f, 1.0f});
        glm::mat4 upscale = glm::scale(glm::mat4{1.0f}, glm::vec3{10.0f, 10.0f, 10.0f});
        glm::mat4 move = glm::translate(glm::mat4{1.0f}, glm::vec3{factor * fish().real(), factor * fish().imag(), 1.0f});
        return(move * upscale * turn * orient);
    }

    void draw() {
        colpickr.colour_fbuffer.bind();
        colpickr.draw(fish_mesh, model(1.0f), 3339895125, cam);
        colpickr.draw(fish_mesh, model(-1.0f), 500, cam);
        glFlush();
        glFinish();
        double mouse_x; double mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
        int under_mouse_x = static_cast<int>(mouse_x);
        int under_mouse_y = win.height - mouse_y;
        std::uint32_t id_under_cursor;
        glReadPixels(under_mouse_x, under_mouse_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &id_under_cursor);
        spdlog::debug("Under cursor: {}", id_under_cursor);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        colpickr.draw(fish_mesh, model(1.0f), 3339895125, cam);
        colpickr.draw(fish_mesh, model(-1.0f), 500, cam);
        
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //terrain.render(cam);
        //meshr.draw(fish_mesh, model(1.0f), cam);
        //meshr.draw(fish_mesh, model(-1.0f), cam);
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
