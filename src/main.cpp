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
#include <te/sim.hpp>
#include <te/gl.hpp>
#include <te/util.hpp>
#include <te/unique_any.hpp>

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

//TODO: get rid of
using namespace te;

// Client Components
struct render_tex {
    std::string filename;
};
struct render_mesh {
    std::string filename;
};
struct pickable {
    std::uint32_t id;
};

double fps = 0.0;

struct sim {
    std::random_device seed_device;
    std::default_random_engine rengine;
    entt::registry entities;
    std::vector<entt::registry::entity_type> commodities;
    std::vector<entt::registry::entity_type> site_blueprints;
    std::unordered_map<glm::ivec2, entt::registry::entity_type> map;
};

#include <te/loader.hpp>
template<>
te::gl::texture2d te::load_from_file(te::gl::context& gl, std::string name) {
    return gl.make_texture(name);
}
template<>
te::mesh te::load_from_file(te::gl::context& gl, std::string name) {
    return te::load_mesh(gl, name, te::gl::common_attribute_locations);
}

struct client {
    // Sim
    std::random_device seed_device;
    std::default_random_engine rengine;
    entt::registry entities;
    std::vector<entt::registry::entity_type> commodities;
    std::vector<entt::registry::entity_type> site_blueprints;
    std::unordered_map<glm::ivec2, entt::registry::entity_type> map;

    // Render
    te::glfw_context glfw;
    te::window win;
    te::loader resources;
    ImGuiIO& imgui_io;
    te::camera cam;
    te::terrain_renderer terrain_renderer;
    te::mesh_renderer mesh_renderer;
    te::colour_picker colour_picker;
    std::uint32_t pick_id = 1;
    
    std::optional<entt::registry::entity_type> entity_under_cursor;
    
    client() :
        rengine { seed_device() },
        win { glfw.make_window(1024, 768, "Hello, World!")},
        resources { win.gl },
        imgui_io { setup_imgui(win) },
        cam {
            {0.0f, 0.0f, 0.0f},
            {-0.7f, -0.7f, 1.0f},
            8.0f
        },
        terrain_renderer{win.gl, rengine, 20, 20},
        mesh_renderer{win.gl},
        colour_picker{win}
        {
        init_blueprints();
        generate_map();
        win.on_key.connect([&](int key, int scancode, int action, int mods){ on_key(key, scancode, action, mods); });
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    
    void init_blueprints() {
        auto wheat = commodities.emplace_back(entities.create());
        entities.assign<commodity>(wheat, "Wheat", 15.0f);
        entities.assign<render_tex>(wheat, "media/wheat.png");
        
        auto barley = commodities.emplace_back(entities.create());
        entities.assign<commodity>(barley, "Barley", 10.0f);
        entities.assign<render_tex>(barley, "media/wheat.png");
        
        auto wheat_field = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(wheat_field, "Wheat Field", glm::ivec2{2,2});
        entities.assign<generator>(wheat_field, wheat, 0.05);
        entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
        entities.assign<pickable>(wheat_field);

        auto barley_field = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(barley_field, "Barley Field", glm::ivec2{2,2});
        entities.assign<generator>(barley_field, barley, 0.01);
        entities.assign<render_mesh>(barley_field, "media/wheat.glb");
        entities.assign<pickable>(barley_field);

        auto dwelling = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(dwelling, "Dwelling", glm::ivec2{1,1});
        demander& dwelling_demands = entities.assign<demander>(dwelling);
        dwelling_demands.demand[wheat] = 50.0f;
        dwelling_demands.demand[barley] = 10.0f;
        entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
        entities.assign<pickable>(dwelling);

        auto market = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(market, "Market", glm::ivec2{2,2});
        entities.assign<te::market>(market);
        entities.assign<render_mesh>(market, "media/market.glb");
        entities.assign<pickable>(market);
    }

    void generate_map() {
        std::discrete_distribution<std::size_t> select_blueprint {5, 5, 22, 1};
        for (int i = 0; i < 50; i++) spawn(site_blueprints[select_blueprint(rengine)]);
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

    glm::vec3 to_world(site place, site_blueprint print) {
        glm::vec3 world_position = to_world(place.position);
        glm::vec3 world_dimensions {
            static_cast<float>(print.dimensions.x),
            static_cast<float>(print.dimensions.y),
            0.0f
        };
        return (2.0f * world_position + world_dimensions) / 2.0f;
    }

    glm::mat4 map_to_model_matrix(site place, site_blueprint print) {
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
        mat4 move = translate (mat4 {1.0f}, to_world(place, print));
        return move * mat4_cast(rotate_variation) * resize * mat4_cast(rotate_zup);
    }

    bool in_market(site question_site, site market_site, market the_market) {
        return glm::length(glm::vec2{question_site.position - market_site.position}) <= the_market.radius;
    }

    bool can_place(entt::registry::entity_type entity, glm::ivec2 where) {
        {
            auto& blueprint = entities.get<site_blueprint>(entity);
            for (int x = 0; x < blueprint.dimensions.x; x++)
                for (int y = 0; y < blueprint.dimensions.y; y++)
                    if (map.find({where.x + x, where.y + y}) != map.end())
                        return false;
        }
        if (auto maybe_market = entities.try_get<market>(entity); maybe_market) {
            auto other_markets = entities.view<site, market>();
            const bool conflict = std::any_of (
                other_markets.begin(),
                other_markets.end(),
                [&] (auto other) {
                    const auto& [other_site, other_market] = other_markets.get<site, market>(other);
                    return glm::length(glm::vec2{where - other_site.position})
                        <= (maybe_market->radius + other_market.radius);
                }
            );
            if (conflict) return false;
        }
        return true;
    }

    void place(entt::registry::entity_type proto, glm::ivec2 where) {
        auto instantiated = entities.create(proto, entities);
        entities.assign<site>(instantiated, where, 0.0f);
        auto& print = entities.get<site_blueprint>(instantiated);
        for (int x = 0; x < print.dimensions.x; x++) {
            for (int y = 0; y < print.dimensions.y; y++) {
                map[{where.x + x, where.y + y}] = instantiated;
            }
        }
        auto& pick = entities.get<pickable>(instantiated);
        pick.id = pick_id++;
    }

    void spawn(entt::registry::entity_type proto) {
        auto print = entities.get<site_blueprint>(proto);
        std::uniform_int_distribution select_x_pos {0, terrain_renderer.width - print.dimensions.x};
        std::uniform_int_distribution select_y_pos {0, terrain_renderer.height - print.dimensions.y};
    try_again:
        glm::ivec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
        if (!can_place(proto, map_pos))
            goto try_again;
        place(proto, map_pos);
    }

    void render_colourpick() {
        colour_picker.colour_fbuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        entities.view<site, site_blueprint, pickable, render_mesh>().each(
            [&](auto& map_site, auto& print, auto& pick, auto& mesh) {
                colour_picker.draw(resources.lazy_load<te::mesh>(mesh.filename), map_to_model_matrix(map_site, print), pick.id, cam);
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
        auto pickable_entities = entities.view<pickable>();
        auto entity_it = std::find_if (
            pickable_entities.begin(),
            pickable_entities.end(),
            [&,id_under_cursor](auto entity) {
                return (pickable_entities.get<pickable>(entity)).id == id_under_cursor;
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
        if (entity_under_cursor && entities.has<market, site>(*entity_under_cursor)) {
            auto [the_market, market_site] = entities.get<market, site>(*entity_under_cursor);
            entities.view<site, site_blueprint, render_mesh>().each([&](auto& map_site, auto& print, auto& rmesh) {
                auto tint = in_market(map_site, market_site, the_market) ? glm::vec4{1.0f, 0.0f, 0.0f, 0.0f} : glm::vec4{0.0f};
                mesh_renderer.draw(resources.lazy_load<te::mesh>(rmesh.filename), map_to_model_matrix(map_site, print), cam, tint);
            });
        } else {
            entities.view<site, site_blueprint, render_mesh>().each([&](auto& map_site, auto& print, auto& rmesh) {
                mesh_renderer.draw(resources.lazy_load<te::mesh>(rmesh.filename), map_to_model_matrix(map_site, print), cam);
            });
        }
    }

    void render_ui() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Text("FPS: %f", fps);
        //TODO: figure out why we need to check if it has a site.
        if (entity_under_cursor && entities.has<site>(*entity_under_cursor)) {
            ImGui::Separator();
            auto map_site = entities.get<site>(*entity_under_cursor);
            ImGui::Text("Map position: (%d, %d)", map_site.position.x, map_site.position.y);
            {
                const auto& [pick, print] = entities.get<pickable, site_blueprint>(*entity_under_cursor);
                std::string building_text = fmt::format("{} (#{})", print.name, pick.id);
                ImGui::Text(building_text.c_str());
            }
            if (auto the_generator = entities.try_get<generator>(*entity_under_cursor); the_generator) {
                ImGui::Separator();
                auto& rendr_tex = entities.get<render_tex>(the_generator->output);
                ImGui::Image((void*)(*resources.lazy_load<te::gl::texture2d>(rendr_tex.filename).hnd), ImVec2{24, 24});
                ImGui::SameLine();
                auto& output_commodity = entities.get<commodity>(the_generator->output);
                ImGui::Text("%d/%d×%s", the_generator->stock, the_generator->max_stock, output_commodity.name.c_str());
                ImGui::SameLine();
                ImGui::ProgressBar(the_generator->progress);
            }
            if (auto the_demands = entities.try_get<demander>(*entity_under_cursor); the_demands) {
                ImGui::Separator();
                for (auto [demanded, demand_level] : the_demands->demand) {
                    auto [demanded_commodity, commodity_tex] = entities.get<commodity, render_tex>(demanded);
                    ImGui::Image((void*)(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd), ImVec2{24, 24}, ImVec2{0, 0}, ImVec2{1, 1}, ImVec4{1, 1, 1, 1}, ImVec4{0, 0, 0, 1});
                    ImGui::SameLine();
                    ImGui::Text("%s: %f", demanded_commodity.name.c_str(), demand_level);
                }
            }
            if (auto the_market = entities.try_get<market>(*entity_under_cursor); the_market) {
                ImGui::Separator();
                for (auto bid : the_market->bids) {
                    auto [bid_commodity, commodity_tex] = entities.get<commodity, render_tex>(bid.good);
                    ImGui::Image((void*)(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd), ImVec2{24, 24});
                    ImGui::SameLine();
                    ImGui::Text("Buy %s at %f", bid_commodity.name.c_str(), bid.price);
                }
            }
        }
        ImGui::Separator();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void input() {
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

    market* market_at(glm::ivec2 x) {
        auto markets = entities.view<market, site>();
        auto it = std::find_if (
            markets.begin(),
            markets.end(),
            [&](auto& e) {
                auto& market_site = markets.get<site>(e);
                auto& the_market = markets.get<market>(e);
                return glm::length(glm::vec2{markets.get<site>(e).position - x}) <= the_market.radius;
            }
        );
        if (it != markets.end()) {
            return &markets.get<market>(*it);
        } else {
            return nullptr;
        };
    }

    void tick() {
        entities.view<generator, site>().each (
            [&](auto& generator, auto& generator_site) {
                if (generator.progress >= 1.0f) {
                    generator.progress -= 1.0f;
                    generator.stock++;
                    if (market* the_market = market_at(generator_site.position); the_market) {
                        auto& output_commodity = entities.get<commodity>(generator.output);
                        the_market->bids.push_back({generator.output, output_commodity.base_price});
                    }
                }
                if (generator.stock < generator.max_stock) {
                    generator.progress += generator.rate;
                }
            }
        );
    }

    void draw() {
        render_colourpick();
        render_scene();
        render_ui();
    }

    void run() {
        auto then = std::chrono::high_resolution_clock::now();
        int frames = 0;
        while (!glfwWindowShouldClose(win.hnd.get())) {
            input();
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
