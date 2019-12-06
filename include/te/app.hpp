#ifndef TE_APP_HPP_INCLUDED
#define TE_APP_HPP_INCLUDED
#include <te/sim.hpp>
#include <random>
#include <te/window.hpp>
#include <te/loader.hpp>
#include <imgui.h>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/colour_picker.hpp>
namespace te {
    struct app {
        te::sim& model;
        std::default_random_engine rengine;
        te::glfw_context glfw;
        te::window win;
        te::loader resources;
        ImGuiIO& imgui_io;
        te::camera cam;
        te::terrain_renderer terrain_renderer;
        te::mesh_renderer mesh_renderer;
        te::colour_picker colour_picker;
        std::optional<entt::registry::entity_type> inspected;

        app(te::sim& model, unsigned int seed);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);
        
        void mouse_pick();
        glm::vec3 to_world(glm::ivec2 map);
        glm::vec3 to_world(te::site place, te::site_blueprint print);
        glm::mat4 map_to_model_matrix(te::site place, te::site_blueprint print);
        void render_scene();
        void render_ui();
        void input();
        void draw();
        void run();
    };
}
#endif
