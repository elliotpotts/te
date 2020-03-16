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
#include <fmod.hpp>
#include <te/fmod.hpp>
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
        te::fmod_system_hnd fmod;
        te::asset_loader loader;
        te::cache<asset_loader> resources;

        std::optional<entt::entity> inspected;
        std::optional<entt::entity> ghost;
        entt::entity marker;

        app(te::sim& model, unsigned int seed);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);

        std::optional<glm::vec2> pos_under_mouse;
        void mouse_pick();
        glm::vec2 cast_ray(glm::vec2 screen_space) const;

        void render_scene();
        void imgui_commicon(entt::entity, ImVec4 colour = ImVec4{1, 1, 1, 1});
        void render_mappos_inspector(const te::site&, const te::named&);
        void render_generator_inspector(const te::generator&);
        void render_demander_inspector(const te::demander&);
        void render_inventory_inspector(const te::inventory&);
        void render_trader_inspector(const te::trader&);
        void render_producer_inspector(const te::producer&, const te::inventory&);
        void render_market_inspector(const te::market&);
        void render_inspectors();
        void render_controller();
        void render_ui();

        void input();
        void draw();
        void run();
    };
}
#endif
