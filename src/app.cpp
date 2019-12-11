#include <te/app.hpp>
#include <fmt/format.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <te/maths.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
    double fps = 0.0;
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

te::app::app(te::sim& model, unsigned int seed) :
    model { model },
    rengine { seed },
    win { glfw.make_window(1024, 768, "Hello, World!")},
    imgui_io { setup_imgui(win) },
    cam {
        {0.0f, 0.0f, 0.0f},
        {-0.7f, -0.7f, 1.0f},
        8.0f
    },
    terrain_renderer{ win.gl, rengine, model.map_width, model.map_height },
    instance_renderer { win.gl },
    colour_picker{ win },
    loader { win.gl },
    resources { loader }
{
    win.on_key.connect([&](int key, int scancode, int action, int mods){ on_key(key, scancode, action, mods); });
    win.on_mouse_button.connect([&](int button, int action, int mods) { on_mouse_button(button, action, mods); });
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void te::app::on_key(int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        cam.offset = glm::rotate(cam.offset, -glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
    }
    if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        cam.offset = glm::rotate(cam.offset, glm::half_pi<float>()/4.0f, glm::vec3{0.0f, 0.0f, 1.0f});
    }
}

void te::app::on_mouse_button(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        mouse_pick();
    }
}


glm::mat4 rotate_zup = glm::mat4_cast(te::rotation_between_units (
    glm::vec3 {0.0f, 1.0f, 0.0f},
    glm::vec3 {0.0f, 0.0f, 1.0f}
));

void te::app::mouse_pick() {
    colour_picker.colour_fbuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    model.entities.view<te::site, te::site_blueprint, te::pickable, te::render_mesh>().each (
        [&](const entt::entity& entity, auto& map_site, auto& print, auto pick, auto& mesh) {
            auto& doc = resources.lazy_load<te::gltf>(mesh.filename);
            auto& current_print = model.entities.get<te::site_blueprint>(entity);
            auto& current_site = model.entities.get<te::site>(entity);
            const auto model_mat = glm::translate (
                glm::vec3 {
                    terrain_renderer.grid_topleft.x + current_site.position.x + current_print.dimensions.x / 2.0f,
                    terrain_renderer.grid_topleft.y + current_site.position.y + current_print.dimensions.y / 2.0f,
                    0.0f
                }
            ) * rotate_zup;
            auto pick_colour = *reinterpret_cast<const std::uint32_t*>(&entity);
            colour_picker.draw(*doc.primitives.begin(), model_mat, pick_colour, cam);
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
    if (model.entities.valid(under_cursor) && model.entities.has<te::pickable>(under_cursor)) {
        inspected = under_cursor;
    } else {
        inspected.reset();
    }
}

void te::app::render_scene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    terrain_renderer.render(cam);

    auto instances = model.entities.group<render_mesh, site, site_blueprint>();
    instances.sort<te::render_mesh> (
        [](const auto& lhs, const auto& rhs) {
            return lhs.filename < rhs.filename;
        }
    );

    const auto begin = instances.begin();
    const auto end = instances.end();
    auto it = begin;
    
    do {
        std::vector<glm::vec2> positions;
        const auto& current_rmesh = instances.get<render_mesh>(*it);
        const auto& current_print = instances.get<site_blueprint>(*it);
        while (it != end && instances.get<render_mesh>(*it).filename == current_rmesh.filename) {
            positions.push_back(instances.get<site>(*it).position);
            it++;
        }
        auto& doc = resources.lazy_load<gltf>(current_rmesh.filename);
        auto& instanced = instance_renderer.instance(*doc.primitives.begin());
        instanced.instance_attribute_buffer.bind();
        glBufferData (
            GL_ARRAY_BUFFER,
            positions.size() * sizeof(decltype(positions)::value_type),
            positions.data(),
            GL_STATIC_READ
        );
        const auto model_mat = glm::translate (
            glm::vec3 {
                terrain_renderer.grid_topleft.x + current_print.dimensions.x / 2.0f,
                terrain_renderer.grid_topleft.y + current_print.dimensions.y / 2.0f,
                0.0f
            }
        ) * rotate_zup;
        instance_renderer.draw(instanced, model_mat, cam, positions.size());
    } while (it != end);
}

namespace {
    void render_ui_demo() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void te::app::render_ui() {
    //render_ui_demo(); return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Text("FPS: %f", fps);
    if (inspected) {
        auto id_string = fmt::format("{}", *reinterpret_cast<std::uint32_t*>(&*inspected));
        ImGui::Separator();
        if (auto [maybe_site, maybe_print] = model.entities.try_get<te::site, te::site_blueprint>(*inspected); maybe_site && maybe_print) {
            ImGui::Text("Map position: (%f, %f)", maybe_site->position.x, maybe_site->position.y);
            ImGui::Text("%s (#%s)", maybe_print->name.c_str(), id_string.c_str());
        }
        if (auto the_generator = model.entities.try_get<te::generator>(*inspected); the_generator) {
            ImGui::Separator();
            auto& rendr_tex = model.entities.get<te::render_tex>(the_generator->output);
            ImGui::Image(*resources.lazy_load<te::gl::texture2d>(rendr_tex.filename).hnd, ImVec2{24, 24});
            ImGui::SameLine();
            auto& output_commodity = model.entities.get<te::commodity>(the_generator->output);
            ImGui::Text(fmt::format("{} @ {}/s", output_commodity.name, the_generator->rate).c_str());
            ImGui::SameLine();
            ImGui::ProgressBar(the_generator->progress);
        }
        if (auto the_demands = model.entities.try_get<te::demander>(*inspected); the_demands) {
            ImGui::Separator();
            for (auto [demanded, demand_level] : the_demands->demand) {
                auto [demanded_commodity, commodity_tex] = model.entities.get<te::commodity, te::render_tex>(demanded);
                ImGui::Image(*resources.lazy_load<te::gl::texture2d>(commodity_tex.filename).hnd, ImVec2{24, 24});
                ImGui::SameLine();
                ImGui::Text("%s: %f", demanded_commodity.name.c_str(), demand_level);
            }
        }
        if (auto the_market = model.entities.try_get<te::market>(*inspected); the_market) {
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

            ImGui::NextColumn();

            ImGui::Text("Demand");
            ImGui::SetColumnWidth(4, 100);
            width_available -= 100;
            ImGui::NextColumn();

            ImGui::SetColumnWidth(3, width_available);
            for (auto [commodity_entity, price] : the_market->prices) {
                auto [the_commodity, commodity_tex] = model.entities.get<te::commodity, te::render_tex>(commodity_entity);
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

                ImGui::Text(fmt::format("{}x", static_cast<int>(commodity_demand)).c_str());
                ImGui::NextColumn();
            }
            ImGui::Columns();
        }
    }
    ImGui::Separator();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void te::app::input() {
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

void te::app::draw() {
    render_scene();
    render_ui();
}

void te::app::run() {
    auto then = std::chrono::high_resolution_clock::now();
    int frames = 0;
    while (!glfwWindowShouldClose(win.hnd.get())) {
        input();
        model.tick();
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
