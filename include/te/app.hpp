#ifndef TE_APP_HPP_INCLUDED
#define TE_APP_HPP_INCLUDED
#include <te/sim.hpp>
#include <random>
#include <te/window.hpp>
#include <te/cache.hpp>
#include <imgui.h>
#include <te/camera.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/instance_renderer.hpp>
#include <te/colour_picker.hpp>
#include <te/util.hpp>
namespace te {
    struct asset_loader {
        te::gl::context& gl;
        te::mesh_renderer& mesh_renderer;
        te::instance_renderer& instance_renderer;
        inline te::mesh operator()(type_tag<te::mesh>, const std::string& filename) {
            return mesh_renderer.load_from_file(filename);
        }
        inline te::gl::texture2d operator()(type_tag<te::gl::texture2d>, const std::string& filename) {
            return gl.make_texture(filename);
        }
        inline te::instanced_primitive operator()(type_tag<te::instanced_primitive>, const std::string& filename) {
            return instance_renderer.load_from_file(filename, 200);
        }
    };
    struct app {
        friend class app_loader;
        te::sim& model;
        std::default_random_engine rengine;
        te::glfw_context glfw;
        te::window win;
        ImGuiIO& imgui_io;
        te::camera cam;
        te::terrain_renderer terrain_renderer;
        te::mesh_renderer mesh_renderer;
        te::instance_renderer instance_renderer;
        te::colour_picker colour_picker;
        asset_loader loader;
        te::cache<asset_loader> resources;
        std::optional<entt::registry::entity_type> inspected;

        app(te::sim& model, unsigned int seed);

        void on_key(int key, int scancode, int action, int mods);
        void on_mouse_button(int button, int action, int mods);
        
        void mouse_pick();
        glm::vec3 to_world(glm::ivec2 map);
        glm::vec3 to_world(te::site place, te::site_blueprint print);
        void render_scene();
        void render_ui();
        void input();
        void draw();
        void run();
    };
}
#endif
