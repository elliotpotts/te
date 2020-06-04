#ifndef TE_APP_HPP_INCLUDED
#define TE_APP_HPP_INCLUDED
#include <te/sim.hpp>
#include <te/client.hpp>
#include <te/server.hpp>
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
#include <te/components.hpp>
#include <te/tween.hpp>
#include <complex>

namespace te {
    struct app {
        // shared between scenes
        std::default_random_engine rengine;
        te::glfw_context glfw;
        te::window win;
        te::fmod_system_hnd fmod;
        ImGuiIO& imgui_io;
        te::asset_loader loader;
        te::cache<asset_loader> resources;

        ISteamNetworkingSockets* netio;
        std::optional<te::server> server;
        SteamNetworkingIPAddr server_addr;
        std::optional<te::client> client;

        // menu scene
        FMOD::Sound* menu_music_src;
        FMOD::Channel* menu_music;

        // gameplay scene
        te::sim& model;
        te::tween<std::complex<double>> radius_tween;
        te::tween<double> zoom_tween;
        te::camera cam;
        te::terrain_renderer terrain_renderer;
        te::mesh_renderer mesh_renderer;

        std::optional<entt::entity> inspected;
        std::optional<entt::entity> ghost;

        struct console_line {
            ImColor prefix_colour;
            std::string prefix;
            std::string content;
        };
        std::vector<console_line> console;

        app(te::sim& model, SteamNetworkingIPAddr server_addr);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);
        void on_chat(te::chat);

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

        bool scroll_console_to_bottom = false;
        void render_console();

        std::optional<entt::entity> merchant_ordering;
        std::optional<entt::entity> merchant_selected;
        void render_merchant_summary(entt::entity);
        void render_orders_controller();
        void render_roster_controller();
        void render_merchants_controller();
        void render_routes_controller();
        void render_construction_controller();
        void render_technology_controller();
        void render_controller();

        bool render_main_menu();

        void render_ui();

        void playsfx(std::string filename);

        void input();
        void draw();
        void run();
    };
}
#endif
