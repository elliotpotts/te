#include <te/app.hpp>
#include <te/util.hpp>
#include <te/server.hpp>
#include <te/client.hpp>
#include <te/net.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <variant>
#include <algorithm>
#include <te/maths.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

te::app::app(te::sim& model, SteamNetworkingIPAddr server_addr) :
    rengine { 42 },
    win { glfw.make_window(1024, 768, "Trade Empires", false)},
    fmod { te::make_fmod_system() },
    loader { win.gl, *fmod },
    resources { loader },

    netio { SteamNetworkingSockets() },
    server_addr { server_addr },

    model { model },
    radius_tween {{std::polar(0.6, glm::half_pi<double>() / 2.0)}, 20},
    zoom_tween {12.0, 20},
    cam {
        {0.0f, 0.0f, 0.0f}, // focus
        0.6, // altitude
        radius_tween.end,
        zoom_tween.end,
        static_cast<float>(win.width()) / win.height() // aspect ratio
    },
    terrain_renderer{ win.gl, rengine, model.map_width, model.map_height },
    mesh_renderer { win.gl },
    ui_renderer { win },
    ui { model, win, input_bus, ui_renderer, resources }
{
    win.set_cursor(make_bitmap("assets/ui/cursor.png"));
    fmod->createStream("assets/music/main-theme.ogg", FMOD_CREATESTREAM | FMOD_LOOP_NORMAL, nullptr, &menu_music_src);
    //fmod->playSound(menu_music_src, nullptr, false, &menu_music);
    win.on_framebuffer_size.connect([&](int width, int height) {
        cam.aspect_ratio = static_cast<float>(width) / height;
        glViewport(0, 0, width, height);
    });
    win.set_attribute(GLFW_RESIZABLE, GLFW_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    model.on_trade.connect([&]() {
        static std::uniform_int_distribution select{1, 4};
        playsfx(fmt::format("assets/sfx/coin{}.wav", select(rengine)));
    });

    // start singleplayer game
    server.emplace(netio, te::port);
    server->max_players = 1;
    client.emplace(server->make_local(model));
    client->send(hello{1, "SinglePringle"});

    win.on_char.connect([&](unsigned int code) {
        ui.on_char(code);
    });
    win.on_key.connect([&](int key, int scancode, int action, int mods) {
        ui.on_key(key, scancode, action, mods);
    });

    ui.behind.on_click.connect([&](){
        if (ghost) {
            auto proto = model.entities.get<te::ghost>(*ghost).proto;
            auto pos = model.entities.get<te::site>(*ghost).position;
            if (model.can_place(proto, pos)) {
                model.entities.destroy(*ghost);
                ghost.reset();
                playsfx("assets/sfx/build2.wav");
                client->send(build {*client->family(), proto, pos} );
                /*
                  auto built = co_await client->build(*client->family(), proto, pos);
                  if (!built) {
                  playsfx("assets/sfx/notif3.wav");
                  }
                */
            } else {
                playsfx("assets/sfx/notif3.wav");
            }
            /* TODO: figure out how to play sound effecs when things fail/don't fail.
             *       Do we always have to wait for a response from the server? */
        } else if (pos_under_mouse) {
            auto map_entities = model.entities.view<te::site, te::pickable>();
            for (auto entity : map_entities) {
                auto& map_site = map_entities.get<te::site>(entity);
                if (glm::distance(map_site.position, *pos_under_mouse) <= 1.0f) {
                    inspected = entity;
                    if (auto gen = model.entities.try_get<te::generator>(entity)) {
                        ui.inspect(entity, *gen);
                    }
                    if (auto mark = model.entities.try_get<te::market>(entity)) {
                        ui.inspect(entity, *mark);
                    }
                    if (auto noisy = model.entities.try_get<te::noisy>(entity); noisy) {
                        playsfx(noisy->filename);
                    }
                    return;
                }
            }
        }
        inspected.reset();
    });
}

#include <complex>
void te::app::on_key(const int key, const int scancode, const int action, const int mods) {
    if (key == GLFW_KEY_ESCAPE) {
        win.close();
        return;
    };
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (!radius_tween) {
            if (key == GLFW_KEY_Q) {
                radius_tween.factor(std::polar(1.0, -glm::half_pi<double>()));
            }
            if (key == GLFW_KEY_E) {
                radius_tween.factor(std::polar(1.0, +glm::half_pi<double>()));
            }
        }
        if (!zoom_tween) {
            if (key == GLFW_KEY_H) zoom_tween.to(glm::clamp(cam.zoom_factor + 4.0f, 4.0f, 20.0f));
            if (key == GLFW_KEY_J) zoom_tween.to(glm::clamp(cam.zoom_factor - 4.0f, 4.0f, 20.0f));
        }
    }
}

void te::app::on_mouse_button(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            playsfx("assets/sfx/unknown1.wav");
        } else if (action == GLFW_RELEASE) {
            playsfx("assets/sfx/unknown2.wav");
        }
    }
}

void te::app::on_chat(te::chat msg) {
    //console.emplace_back(console_line{ImColor{255, 255, 255}, fmt::format("[{}]: ", msg.from), msg.content});
    scroll_console_to_bottom = true;
}

glm::mat4 rotate_zup = glm::mat4_cast(te::rotation_between_units (
    glm::vec3 {0.0f, 1.0f, 0.0f},
    glm::vec3 {0.0f, 0.0f, 1.0f}
));

void te::app::mouse_pick() {
    double mouse_x; double mouse_y;
    glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
    pos_under_mouse = cast_ray({mouse_x, mouse_y});
}

void te::app::render_scene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    terrain_renderer.render(cam);

    auto instances = model.entities.group<render_mesh, site, footprint>();
    instances.sort<te::render_mesh> (
        [](const auto& lhs, const auto& rhs) {
            return lhs.filename < rhs.filename;
        }
    );

    const auto begin = instances.begin();
    const auto end = instances.end();
    auto it = begin;

    const te::market* market = nullptr;
    const te::site* market_site = nullptr;
    if (inspected) {
        market = model.entities.try_get<te::market>(*inspected);
        market_site = model.entities.try_get<te::site>(*inspected);
    }

    while (it != end) {
        std::vector<te::mesh_renderer::instance_attributes> instance_attributes;
        const auto& current_rmesh = instances.get<render_mesh>(*it);
        while (it != end && instances.get<render_mesh>(*it).filename == current_rmesh.filename) {
            bool tinted = (market && market_site && model.in_market(instances.get<site>(*it), *market_site, *market))
                       || inspected == *it;
            instance_attributes.push_back (
                te::mesh_renderer::instance_attributes {
                    instances.get<site>(*it).position,
                    tinted ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f)
                }
            );
            it++;
        }
        auto& doc = resources.lazy_load<gltf>(current_rmesh.filename);
        auto& instanced = mesh_renderer.instance(*doc.primitives.begin());
        instanced.instance_attribute_buffer.bind();
        instanced.instance_attribute_buffer.upload(instance_attributes.begin(), instance_attributes.end());
        mesh_renderer.draw(instanced, rotate_zup, cam, instance_attributes.size());
    }
}

static bool at_main_menu = false;

struct panel {
    std::string_view name;
    int pane;
    int icon;
};

void te::app::render_ui() {
    if (at_main_menu) {
        
    } else {
        ui_renderer.image(resources.lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/168.png"), {0, 0});
        ui_renderer.image(resources.lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/169.png"), {0, 20+693});

        ui_renderer.text("Barley Field: Grows grain demanded by dwellings, breweries", {274, 736}, {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 6.0});
        ui_renderer.text("and many other structures", {274, 750}, {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 6.0});

        static std::array<panel, 4> panels {{
            panel { "Roster", 11, 176 },
            panel { "Routes", 89, 177},
            panel { "Construction", 2, 178 },
            panel { "Technologies", 136, 179 }
        }};

        static bool mouse_down = false;
        static bool last_mouse_down = false;
        double mouse_x, mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);
        last_mouse_down = mouse_down;
        mouse_down = glfwGetMouseButton(win.hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        glm::vec2 cursor {28.0f, 727.0f};
        static std::optional<int> panel_opened;
        for (int i = 0; i < 4; i++) {
            if (mouse_down && !last_mouse_down
                && mouse_x >= cursor.x && mouse_x <= cursor.x + 35.0f
                && mouse_y >= cursor.y && mouse_y <= cursor.y + 33.0f)
            {
                if (panel_opened == i) {
                    panel_opened.reset();
                } else {
                    panel_opened = i;
                }
            }
            panel& panel_i = panels[i];
            if (panel_opened == i) {
                ui_renderer.image(resources.lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{:>03}.png", panel_i.pane)), {0, 20});
                ui_renderer.image(resources.lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{:>03}.png", panel_i.icon)), cursor, {35, 33}, {0.5f, 0.0f}, {0.5f, 1.0f});
            } else {
                ui_renderer.image(resources.lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{:>03}.png", panel_i.icon)), cursor, {35, 33}, {0.0f, 0.0f}, {0.5f, 1.0f});
            }
            cursor.x += 56.0f;
        }

        ui.render();

        /*
        ui.centered_text("CONSTRUCTION", {45, 53}, 195.0, 8.0);

        static int selected = 0;
        static std::vector<std::string_view> strs {
            "PATHWAYS", "MARKETS", "DEPOTS", "PRODUCTION BUILDINGS", "DEMAND BUILDINGS"
        };
        double mouse_x; double mouse_y;
        glfwGetCursorPos(win.hnd.get(), &mouse_x, &mouse_y);

        const glm::vec2 row_dims {243, 16};
        const glm::vec2 border_dims {243, 6};
        const glm::vec2 row_txt {243, 11};
        glm::vec2 row_i {13, 88};
        for (unsigned i = 0; i < strs.size(); i++) {
            const glm::vec2 tl = row_i;
            const glm::vec2 br {row_i.x + row_dims.x, row_i.y + row_dims.y};
            if (glfwGetMouseButton(win.hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
                && mouse_x >= tl.x && mouse_x <= br.x && mouse_y >= tl.y && mouse_y <= br.y
                && selected != i) {
                selected = i;
            }
            if (i == selected) {
                ui.texquad(resources.lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/095.png"), tl, row_dims);
            }
            const glm::vec2 text_start {row_i.x, row_i.y + row_txt.y};
            ui.centered_text(strs[i], text_start, row_txt.x, 6.0);
            row_i.y += row_dims.y + border_dims.y;
        }
        */

        //render_inspectors();
        //render_controller();
    }
}

void te::app::playsfx(std::string filename) {
    fmod->playSound(resources.lazy_load<te::fmod_sound_hnd>(filename).get(), nullptr, false, nullptr);
}

void te::app::input() {
    if (!ui.input()) {
        mouse_pick();
        if (ghost && pos_under_mouse) {
            model.entities.emplace_or_replace<site>(*ghost, model.snap(*pos_under_mouse, model.entities.get<footprint>(*ghost).dimensions));
        }
    }
}

void te::app::draw() {
    if (radius_tween) {
        cam.radius = radius_tween();
    }
    if (zoom_tween) {
        cam.zoom_factor = zoom_tween();
    }
    render_scene();
    glDisable(GL_DEPTH_TEST);
    render_ui();
    glEnable(GL_DEPTH_TEST);
}

double fps = 0.0;

void te::app::run() {
    glEnable(GL_MULTISAMPLE);
    auto then = std::chrono::high_resolution_clock::now();
    int frames = 0;
    while (!glfwWindowShouldClose(win.hnd.get())) {
        input();
        if (frames == 30) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - then;
            if (server) server->poll(elapsed.count());
            if (client) client->poll(elapsed.count());
            fps = static_cast<double>(frames) / elapsed.count();
            spdlog::debug("fps: {}", fps);
            frames = 0;
            then = std::chrono::high_resolution_clock::now();
        }
        draw();
        glfwSwapBuffers(win.hnd.get());
        frames++;
        glfwPollEvents();
    }
}

glm::vec2 te::app::cast_ray(glm::vec2 ray_screen) const {
    const glm::vec3 ray_ndc {
        (2.0f * ray_screen.x) / win.width() - 1.0f,
        1.0f - (2.0f * ray_screen.y) / win.height(),
        1.0f
    };
    auto [ray_origin, ray_direction] = cam.cast(ray_ndc);

    const glm::vec3 ground_normal {0.0f, 0.0f, 1.0f};
    const auto t = -dot(ray_origin, ground_normal) / dot(ray_direction, ground_normal);
    const glm::vec3 intersection = ray_origin + ray_direction * t;
    return glm::vec2{intersection.x, intersection.y};
}
