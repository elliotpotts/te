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
#include <te/loader.hpp>
#include <cmath>

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
};

std::random_device seed_device;
double fps = 0.0;

struct model {
    std::default_random_engine rengine;
    entt::registry entities;
    std::vector<entt::registry::entity_type> site_blueprints;

    const int map_width = 20;
    const int map_height = 20;
    std::unordered_map<glm::ivec2, entt::registry::entity_type> map;

    model() : rengine { seed_device() } {
        init_blueprints();
        generate_map();
    }

    void init_blueprints() {
        // Commodities
        auto wheat = entities.create();
        entities.assign<commodity>(wheat, "Wheat", 15.0f);
        entities.assign<render_tex>(wheat, "media/wheat.png");

        auto barley = entities.create();
        entities.assign<commodity>(barley, "Barley", 10.0f);
        entities.assign<render_tex>(barley, "media/wheat.png");

        std::unordered_map<entt::entity, double> base_market_prices;
        entities.view<commodity>().each([&](const entt::entity e, commodity& comm) {
            base_market_prices[e] = comm.base_price;
        });

        // Buildings
        auto wheat_field = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(wheat_field, "Wheat Field", glm::ivec2{2,2});
        entities.assign<generator>(wheat_field, wheat, 0.01);
        entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
        entities.assign<pickable>(wheat_field);

        auto barley_field = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(barley_field, "Barley Field", glm::ivec2{2,2});
        entities.assign<generator>(barley_field, barley, 0.005);
        entities.assign<render_mesh>(barley_field, "media/wheat.glb");
        entities.assign<pickable>(barley_field);

        auto dwelling = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(dwelling, "Dwelling", glm::ivec2{1,1});
        demander& dwelling_demands = entities.assign<demander>(dwelling);
        dwelling_demands.demand[wheat] = 0.0005f;
        dwelling_demands.demand[barley] = 0.0001f;
        entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
        entities.assign<pickable>(dwelling);

        auto market = site_blueprints.emplace_back(entities.create());
        entities.assign<site_blueprint>(market, "Market", glm::ivec2{2,2});
        entities.assign<te::market>(market, base_market_prices);
        entities.assign<render_mesh>(market, "media/market.glb");
        entities.assign<pickable>(market);
    }

    void generate_map() {
        std::discrete_distribution<std::size_t> select_blueprint {5, 5, 22, 1};
        for (int i = 0; i < 50; i++) spawn(site_blueprints[select_blueprint(rengine)]);
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
    }

    void spawn(entt::registry::entity_type proto) {
        auto print = entities.get<site_blueprint>(proto);
        std::uniform_int_distribution select_x_pos {0, map_width - print.dimensions.x};
        std::uniform_int_distribution select_y_pos {0, map_height - print.dimensions.y};
    try_again:
        glm::ivec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
        if (!can_place(proto, map_pos))
            goto try_again;
        place(proto, map_pos);
    }

    market* market_at(glm::ivec2 x) {
        auto markets = entities.view<market, site>();
        auto it = std::find_if (
            markets.begin(),
            markets.end(),
            [&](auto& e) {
                auto& market_site = markets.get<site>(e);
                auto& the_market = markets.get<market>(e);
                return glm::length(glm::vec2{market_site.position - x}) <= the_market.radius;
            }
        );
        if (it != markets.end()) {
            return &markets.get<market>(*it);
        } else {
            return nullptr;
        };
    }


    void tick() {
        entities.view<market, site>().each (
            [&](auto& market, auto& market_site) {
                entities.view<generator, site>().each (
                    [&](auto& generator, auto& generator_site) {
                        if (in_market(generator_site, market_site, market)) {
                            if (generator.progress >= 1.0f) {
                                generator.progress -= 1.0f;
                                generator.stock++;
                                market.stock[generator.output]++;
                            }
                            if (generator.stock < generator.max_stock) {
                                generator.progress += generator.rate;
                            }
                        }
                    }
                );
                entities.view<demander, site>().each (
                    [&](auto& demander, auto& demander_site) {
                        if (in_market(demander_site, market_site, market)) {
                            for (auto [commodity_e, commodity_demand] : demander.demand) {
                                market.demand[commodity_e] += commodity_demand;
                            }
                        }
                    }
                );
            }
        );
    }
};

struct client {
    std::default_random_engine rengine;
    model& sim;
    te::glfw_context glfw;
    te::window win;
    te::loader resources;
    ImGuiIO& imgui_io;
    te::camera cam;
    te::terrain_renderer terrain_renderer;
    te::mesh_renderer mesh_renderer;
    te::colour_picker colour_picker;
    std::optional<entt::registry::entity_type> entity_under_cursor;

    client(model& sim) :
        rengine { seed_device() },
        sim { sim },
        win { glfw.make_window(1024, 768, "Hello, World!")},
        resources { win.gl },
        imgui_io { setup_imgui(win) },
        cam {
            {0.0f, 0.0f, 0.0f},
            {-0.7f, -0.7f, 1.0f},
            8.0f
        },
        terrain_renderer{win.gl, rengine, sim.map_width, sim.map_height},
        mesh_renderer{win.gl},
        colour_picker{win}
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

    void render_colourpick() {
        colour_picker.colour_fbuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        sim.entities.view<site, site_blueprint, pickable, render_mesh>().each(
            [&](const entt::entity& entity, auto& map_site, auto& print, auto pick, auto& mesh) {
                auto pick_colour = *reinterpret_cast<const std::uint32_t*>(&entity);
                colour_picker.draw(resources.lazy_load<te::mesh>(mesh.filename), map_to_model_matrix(map_site, print), pick_colour, cam);
            }
        );
        glFlush();
        glFinish();
        double mouse_x; double mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
        int under_mouse_x = static_cast<int>(mouse_x);
        int under_mouse_y = win.height - mouse_y;
        entt::entity under_cursor;
        win.gl.toggle_perf_warnings(false);
        glReadPixels(under_mouse_x, under_mouse_y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &under_cursor);
        win.gl.toggle_perf_warnings(true);
        if (sim.entities.valid(under_cursor) && sim.entities.has<pickable>(under_cursor)) {
            entity_under_cursor = under_cursor;
        } else {
            entity_under_cursor.reset();
        }
    }

    void render_scene() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        terrain_renderer.render(cam);
        if (entity_under_cursor && sim.entities.has<market, site>(*entity_under_cursor)) {
            auto [the_market, market_site] = sim.entities.get<market, site>(*entity_under_cursor);
            sim.entities.view<site, site_blueprint, render_mesh>().each([&](auto& map_site, auto& print, auto& rmesh) {
                auto tint = sim.in_market(map_site, market_site, the_market) ? glm::vec4{1.0f, 0.0f, 0.0f, 0.0f} : glm::vec4{0.0f};
                mesh_renderer.draw(resources.lazy_load<te::mesh>(rmesh.filename), map_to_model_matrix(map_site, print), cam, tint);
            });
        } else {
            sim.entities.view<site, site_blueprint, render_mesh>().each([&](auto& map_site, auto& print, auto& rmesh) {
                mesh_renderer.draw(resources.lazy_load<te::mesh>(rmesh.filename), map_to_model_matrix(map_site, print), cam);
            });
        }
    }

    void render_ui_demo() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void render_ui() {
        //render_ui_demo(); return;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Text("FPS: %f", fps);
        if (entity_under_cursor) {
            auto id_string = fmt::format("{}", *reinterpret_cast<std::uint32_t*>(&*entity_under_cursor));
            ImGui::Separator();
            if (auto [maybe_site, maybe_print] = sim.entities.try_get<site, site_blueprint>(*entity_under_cursor); maybe_site && maybe_print) {
                ImGui::Text("Map position: (%d, %d)", maybe_site->position.x, maybe_site->position.y);
                ImGui::Text("%s (#%s)", maybe_print->name.c_str(), id_string.c_str());
            }
            if (auto the_generator = sim.entities.try_get<generator>(*entity_under_cursor); the_generator) {
                ImGui::Separator();
                auto& rendr_tex = sim.entities.get<render_tex>(the_generator->output);
                ImGui::Image(*resources.lazy_load<te::gl::texture2d>(rendr_tex.filename).hnd, ImVec2{24, 24});
                ImGui::SameLine();
                auto& output_commodity = sim.entities.get<commodity>(the_generator->output);
                ImGui::Text("%d/%d×%s", the_generator->stock, the_generator->max_stock, output_commodity.name.c_str());
                ImGui::SameLine();
                ImGui::ProgressBar(the_generator->progress);
            }
            if (auto the_demands = sim.entities.try_get<demander>(*entity_under_cursor); the_demands) {
                ImGui::Separator();
                for (auto [demanded, demand_level] : the_demands->demand) {
                    auto [demanded_commodity, commodity_tex] = sim.entities.get<commodity, render_tex>(demanded);
                    ImGui::Image(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd, ImVec2{24, 24});
                    ImGui::SameLine();
                    ImGui::Text("%s: %f", demanded_commodity.name.c_str(), demand_level);
                }
            }
            if (auto the_market = sim.entities.try_get<market>(*entity_under_cursor); the_market) {
                ImGui::Separator();
                ImGui::Columns(5);
                float width_available = ImGui::GetWindowContentRegionWidth();

                ImGui::Text("Stock");
                ImGui::SetColumnWidth(0, 80);
                width_available -= 80;
                ImGui::NextColumn();

                ImGui::SetColumnWidth(1, 50);
                width_available -= 50;
                ImGui::NextColumn();

                ImGui::Text("Commodity");
                ImGui::SetColumnWidth(2, 300);
                width_available -= 300;
                ImGui::NextColumn();

                ImGui::Text("");
                ImGui::NextColumn();

                ImGui::Text("Demand");
                ImGui::SetColumnWidth(4, 100);
                width_available -= 100;
                ImGui::NextColumn();

                ImGui::SetColumnWidth(3, width_available);
                for (auto [commodity_entity, price] : the_market->prices) {
                    auto [the_commodity, commodity_tex] = sim.entities.get<commodity, render_tex>(commodity_entity);
                    ImGui::Text(fmt::format("{}", the_market->stock[commodity_entity]).c_str());
                    ImGui::NextColumn();

                    ImGui::Image(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd, ImVec2{24, 24});
                    ImGui::NextColumn();

                    ImGui::Text(the_commodity.name.c_str());
                    ImGui::NextColumn();

                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    static const auto light_blue = ImColor(ImVec4{22.9/100.0, 60.7/100.0, 85.9/100.0, 1.0f});
                    static const auto dark_blue = ImColor(ImVec4{22.9/255.0, 60.7/255.0, 85.9/255.0, 1.0f});
                    static const auto white = ImColor(ImVec4{1.0, 1.0, 1.0, 1.0});
                    ImVec2 pos {ImGui::GetCursorScreenPos()};
                    float commodity_demand = static_cast<float>(the_market->demand[commodity_entity]);
                    float bar_left = pos.x;
                    float bar_width = width_available - 16;
                    float bar_right = pos.x + bar_width;
                    draw->AddRectFilled (
                        pos,
                        ImVec2{bar_left + bar_width * std::min(commodity_demand, 1.0f), pos.y + 16.0f},
                        light_blue, 0, 0.0f
                    );
                    draw->AddRectFilled (
                        ImVec2{bar_right - bar_width * (std::max(1.0f - commodity_demand, 0.0f)), pos.y},
                        ImVec2{bar_right, pos.y + 16.0f},
                        dark_blue, 0, 0.0f
                    );
                    int marker_count = static_cast<int>(commodity_demand);
                    float gap_size = ((std::floor(commodity_demand) / commodity_demand) * bar_width) / static_cast<float>(marker_count);
                    for (int i = 0; i < marker_count; i++) {
                        draw->AddRectFilled (
                            ImVec2{bar_left + (i + 1) * gap_size - 2.0f, pos.y},
                            ImVec2{bar_left + (i + 1) * gap_size + 2.0f, pos.y + 16.0f},
                            white, 0, 0.0f
                        );
                    }
                    ImGui::NextColumn();

                    ImGui::Text(fmt::format("{:.0f}x", commodity_demand).c_str());
                    ImGui::NextColumn();

                    // ImGui::Image(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd, ImVec2{24, 24});
                    // ImGui::SameLine();
                    // ImGui::Text(fmt::format("{}: ¤{:.2f}", the_commodity.name, price).c_str());
                    // static const auto blue = ImColor(ImVec4{0.0f, 200.0f / 255.0f, 1.0f, 1.0f});
                    // ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    // ImVec2 pos {ImGui::GetCursorScreenPos()};
                    // draw_list->AddRectFilled(ImVec2(pos.x,                                       pos.y),
                    //                          ImVec2(pos.x + ImGui::GetContentRegionAvailWidth(), pos.y + 16.0), blue, 0, 1.0f);
                    // ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 18.0);
                }
                ImGui::Columns();
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
            sim.tick();
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
    model game;
    client the_client { game };
    the_client.run();
    return 0;
}
