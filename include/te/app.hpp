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
#include <glm/glm.hpp>
#include <fmod.hpp>
#include <te/fmod.hpp>
#include <te/components.hpp>
#include <te/tween.hpp>
#include <complex>
#include <te/classic_ui.hpp>
#include <ft/ft.hpp>
#include <te/gl.hpp>
#include <hb/buffer.hpp>
#include <ibus/bus.hpp>

namespace te {
    struct app {
        // shared between scenes
        std::default_random_engine rengine;
        te::glfw_context glfw;
        te::window win;
        ibus::bus input_bus;
        te::fmod_system_hnd fmod;
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

        te::canvas_renderer canvas;
        te::ui::root ui;

        std::optional<entt::entity> inspected;
        std::optional<entt::entity> ghost;

        app(te::sim& model, SteamNetworkingIPAddr server_addr);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);
        void on_chat(te::chat);

        std::optional<glm::vec2> pos_under_mouse;
        std::optional<entt::entity> entt_under_mouse;
        void mouse_pick();
        glm::vec2 cast_ray(glm::vec2 screen_space) const;

        void render_scene();

        std::optional<entt::entity> merchant_ordering;
        std::optional<entt::entity> merchant_selected;

        void playsfx(std::string filename);

        void input();
        void draw();
        void run();
    };
}
#endif
