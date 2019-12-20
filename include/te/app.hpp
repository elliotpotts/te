#ifndef TE_APP_HPP_INCLUDED
#define TE_APP_HPP_INCLUDED
#include <te/sim.hpp>
#include <te/window.hpp>
#include <te/cache.hpp>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/colour_picker.hpp>
#include <te/util.hpp>
#include <unordered_map>
#include <random>
#include <imgui.h>
#include <glm/glm.hpp>
namespace te {
    struct app {
        te::sim& model;
        std::default_random_engine rengine;
        te::glfw_context glfw;
        te::window win;
        ImGuiIO& imgui_io;
        te::camera cam;
        te::terrain_renderer terrain_renderer;
        te::mesh_renderer mesh_renderer;
        te::colour_picker colour_picker;
        te::asset_loader loader;
        te::cache<asset_loader> resources;

        std::optional<entt::entity> inspected;
        std::optional<entt::entity> ghost;

        glm::mat4 model_tfm(te::site_blueprint) const;

        app(te::sim& model, unsigned int seed);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);

        void mouse_pick();
        void render_scene();

        void render_inspector();
        void render_controller();
        void render_ui();

        void input();
        void draw();
        void run();
    };
}
#endif
