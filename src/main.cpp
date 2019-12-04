#include <random>
#include <te/window.hpp>
#include <te/terrain_renderer.hpp>
#include <te/mesh_renderer.hpp>
#include <te/colour_picker.hpp>
#include <glad/glad.h>
#include <chrono>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <entt/entt.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>

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

struct blueprint {
    std::string name;
    te::mesh mesh;
    glm::ivec2 plan;
};
struct commodity {
    std::string name;
    te::gl::texture2d* tex;
};
struct demands {
    std::unordered_map<entt::registry::entity_type, double> demand;
};
struct map_placement {
    glm::ivec2 position;
    float rotation;
    glm::ivec2 dimensions;
};
struct render_mesh {
    te::mesh* mesh;
};
struct pickable_mesh {
    te::mesh* mesh;
    std::uint32_t id;
    std::string name;
};

#include <functional>
namespace std {
    template<>
    struct hash<glm::vec2> {
        std::size_t operator()(glm::vec2 xy) const {
            std::size_t hash_x = std::hash<float>{}(xy.x);
            std::size_t hash_y = std::hash<float>{}(xy.y);
            return hash_x ^ (hash_y << 1);
        }
    };
    template<>
    struct hash<glm::ivec2> {
        std::size_t operator()(glm::vec2 xy) const {
            std::size_t hash_x = std::hash<int>{}(xy.x);
            std::size_t hash_y = std::hash<int>{}(xy.y);
            return hash_x ^ (hash_y << 1);
        }
    };
}

namespace {
    ImGuiIO& setup_imgui(te::window& win) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplGlfw_InitForOpenGL(win.hnd.get(), true);
        ImGui_ImplOpenGL3_Init("#version 130");
        io.Fonts->AddFontFromFileTTF("LiberationSans-Regular.ttf", 18);
        return io;
    }
}

double fps = 0.0;

struct client {
    std::random_device seed_device;
    std::default_random_engine rengine;
    te::glfw_context glfw;
    te::window win;
    ImGuiIO& imgui_io;
    te::camera cam;
    te::terrain_renderer terrain_renderer;
    te::mesh_renderer mesh_renderer;
    te::colour_picker colour_picker;
    std::vector<blueprint> blueprints;
    te::gl::texture<GL_TEXTURE_2D> wheat_tex;
    entt::registry entities;
    std::optional<entt::registry::entity_type> entity_under_cursor;
    std::unordered_map<glm::ivec2, entt::registry::entity_type> map;
    entt::registry::entity_type wheat;
    entt::registry::entity_type barley;

    client() :
        rengine { seed_device() },
        win {glfw.make_window(1024, 768, "Hello, World!")},
        imgui_io {setup_imgui(win)},
        cam {
            {0.0f, 0.0f, 0.0f},
            {-0.7f, -0.7f, 1.0f},
            8.0f
        },
        terrain_renderer(win.gl, rengine, 20, 20),
        mesh_renderer(win.gl),
        colour_picker(win),
        wheat_tex(win.gl.make_texture("media/wheat.png")),
        wheat{entities.create()},
        barley{entities.create()}
        {
        win.on_key.connect([&](int key, int scancode, int action, int mods){ on_key(key, scancode, action, mods); });
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        blueprints.push_back(blueprint{"Wheat Field", te::load_mesh(win.gl, "media/wheat.glb",    te::gl::common_attribute_locations), glm::ivec2{2, 2}});
        blueprints.push_back(blueprint{"Dwelling",    te::load_mesh(win.gl, "media/dwelling.glb", te::gl::common_attribute_locations), glm::ivec2{1, 1}});
        entities.assign<commodity>(wheat, commodity{"Wheat", &wheat_tex});
        entities.assign<commodity>(barley, commodity{"Barley", &wheat_tex});

        for (int i = 0; i < 100; i++) place_random_building();
    }

    void on_key(int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
        if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }

    glm::vec3 to_world(glm::ivec2 map) {
        glm::vec3 world_pos = glm::vec3{ static_cast<float>(map.x), static_cast<float>(map.y), 0.0f }
                            + terrain_renderer.grid_topleft;
        return world_pos;
    }

    glm::vec3 to_world(map_placement place) {
        glm::vec3 world_position = to_world(place.position);
        glm::vec3 world_dimensions {
            static_cast<float>(place.dimensions.x),
            static_cast<float>(place.dimensions.y),
            0.0f
        };
        return (2.0f * world_position + world_dimensions) / 2.0f;
    }

    glm::mat4 map_to_model_matrix(map_placement place) {
        using namespace glm;
        mat4 resize { 1.0f };
        quat rotate_zup = rotation_between_units (
            glm::vec3 {0.0f, 1.0f, 0.0f},
            glm::vec3 {0.0f, 0.0f, 1.0f}
        );
        quat rotate_variation = angleAxis (
            glm::half_pi<float>() * place.rotation,
            glm::vec3 {0.0f, 0.0f, 1.0f}
        );
        mat4 move = translate (mat4 {1.0f}, to_world(place));
        return move * mat4_cast(rotate_variation) * resize * mat4_cast(rotate_zup);
    }

    bool can_place(const blueprint& what, glm::ivec2 where) {
        for (int x = 0; x < what.plan.x; x++)
            for (int y = 0; y < what.plan.y; y++)
                if (map.find({where.x + x, where.y + y}) != map.end())
                    return false;
        return true;
    }

    void place_building(blueprint& what, glm::ivec2 where) {
        static std::uint32_t bldg_id = 1;
        auto bldg = entities.create();
        for (int x = 0; x < what.plan.x; x++)
            for (int y = 0; y < what.plan.y; y++)
                map[glm::ivec2{where.x + x, where.y + y}] = bldg;
        entities.assign<map_placement>(bldg, where, 0.0f, what.plan);
        entities.assign<render_mesh>(bldg, &what.mesh);
        entities.assign<pickable_mesh>(bldg, &what.mesh, bldg_id++, what.name);
        //todo: remove
        std::unordered_map<entt::registry::entity_type, double> demand;
        demand[wheat] = 42.0f;
        static std::bernoulli_distribution flip_coin;
        if (flip_coin(rengine)) {
            demand[barley] = 10.0f;
        }
        entities.assign<demands>(bldg, demands{demand});
    }

    void place_random_building() {
        std::uniform_int_distribution<std::size_t> select_blueprint {0, blueprints.size() - 1};
    try_again:
        auto& blueprint = blueprints[select_blueprint(rengine)];
        std::uniform_int_distribution select_x_pos {0, terrain_renderer.width - blueprint.plan.x};
        std::uniform_int_distribution select_y_pos {0, terrain_renderer.height - blueprint.plan.y};
        glm::ivec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
        if (!can_place(blueprint, map_pos))
            goto try_again;
        place_building(blueprint, map_pos);
    }

    void render_colourpick() {
        colour_picker.colour_fbuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        entities.view<map_placement, pickable_mesh>().each(
            [&](auto& map_place, auto& pick) {
                colour_picker.draw(*pick.mesh, map_to_model_matrix(map_place), pick.id, cam);
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
                entity_under_cursor = *entity_it;
            }
        } else {
            entity_under_cursor.reset();
        }
    }

    void render_scene() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        terrain_renderer.render(cam);
        entities.view<map_placement, render_mesh>().each (
            [&](auto& map_place, auto& rmesh) {
                mesh_renderer.draw(*rmesh.mesh, map_to_model_matrix(map_place), cam);
            }
        );
    }

    void render_ui() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Text("FPS: %f", fps);
        if (entity_under_cursor) {
            ImGui::Separator();
            auto map_place = entities.get<map_placement>(*entity_under_cursor);
            ImGui::Text("Map position: (%d, %d)", map_place.position.x, map_place.position.y);
            {
                auto& pick = entities.get<pickable_mesh>(*entity_under_cursor);
                std::string building_text = fmt::format("{} (#{})", pick.name, pick.id);
                //auto text_size = CalcTextSize(building_text.begin(), building_text.end());
                //auto avail_width = 200;
                ImGui::Text(building_text.c_str());
            }
            if (auto the_demands = entities.try_get<demands>(*entity_under_cursor); the_demands) {
                ImGui::Separator();
                for (auto [demanded_entt, demand_level] : the_demands->demand) {
                    commodity& the_commodity = entities.get<commodity>(demanded_entt);
                    ImGui::Image((void*)(*the_commodity.tex->hnd), ImVec2{24, 24}, ImVec2{0, 0}, ImVec2{1, 1}, ImVec4{1, 1, 1, 1}, ImVec4{0, 0, 0, 1});
                    ImGui::SameLine();
                    ImGui::Text("%s: %f", the_commodity.name.c_str(), demand_level);
                }
            }
        }
        ImGui::Separator();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void draw() {
        render_colourpick();
        render_scene();
        render_ui();
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
                fps = frames / secs.count();
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
