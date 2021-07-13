#include <te/app.hpp>
#include <te/util.hpp>
#include <te/server.hpp>
#include <te/client.hpp>
#include <te/net.hpp>
#include <te/classic_ui.hpp>
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

static std::string ui_img(int i) {
    return fmt::format("assets/a_ui,6.{{}}/{:0>3}.png", i);
}

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
        0.8, // altitude
        radius_tween.end,
        zoom_tween.end,
        static_cast<float>(win.width()) / win.height() // aspect ratio
    },
    terrain_renderer{ win.gl, rengine, model.map_width, model.map_height },
    mesh_renderer { win.gl },
    canvas { win },
    ui { win, canvas, resources }
{
    win.set_cursor(make_bitmap("assets/ui/cursor.png"));
    win.on_key.connect([&](int a, int b, int c, int d) { on_key(a,b,c,d); });
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

    ui.on_click.connect([&](te::ui::node& n, int button, int action, int mods) {
        //TODO: move to global click handler
        if (action == GLFW_PRESS) {
            playsfx("assets/sfx/unknown1.wav");
        } else if (action == GLFW_RELEASE) {
            playsfx("assets/sfx/unknown2.wav");
        }

        if (action == GLFW_RELEASE && entt_under_mouse) {
            inspected = entt_under_mouse;
            if (auto noisy = model.entities.try_get<te::noisy>(*inspected); noisy) {
                noise(noisy->filename);
            }
            if (auto generator = model.entities.try_get<te::generator>(*inspected); generator) {
                auto gen_ui = model.entities.try_get<std::shared_ptr<ui::generator_window>>(*inspected);
                if (gen_ui) {
                    auto it = std::find(ui.children.begin(), ui.children.end(), *gen_ui);
                    if (it != ui.children.end()) {
                        std::rotate(it, it + 1, ui.children.end());
                    } else {
                        ui.children.push_back(*gen_ui);
                    }
                } else {
                    auto created = std::make_shared<ui::generator_window>(model, ui, *inspected);
                    model.entities.emplace<std::shared_ptr<ui::generator_window>>(*inspected, created);
                    ui.children.push_back(created);
                    created->parent = &ui;
                }
            }
        }
        return true;
    });

    // start singleplayer game
    server.emplace(netio, te::port);
    server->max_players = 1;
    client.emplace(server->make_local(model));
    client->send(hello{1, "SinglePringle"});
}

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
}

void te::app::on_chat(te::chat msg) {
    //console.emplace_back(console_line{ImColor{255, 255, 255}, fmt::format("[{}]: ", msg.from), msg.content});
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

void te::app::playsfx(std::string filename) {
    fmod->playSound(resources.lazy_load<te::fmod_sound_hnd>(filename).get(), nullptr, false, nullptr);
}

void te::app::noise(std::string filename) {
    static std::string last_played = "";
    static auto last_played_at = std::chrono::system_clock::now();
    const auto now = std::chrono::system_clock::now();
    if (now - last_played_at >= std::chrono::seconds{2} || filename != last_played) {
        last_played = filename;
        last_played_at = now;
        playsfx(filename);
    }
}

void te::app::input() {
    if (!ui.capture_input()) {
        mouse_pick();
        entt_under_mouse.reset();
        if (pos_under_mouse) {
            if (ghost) {
                model.entities.emplace_or_replace<site>(*ghost, model.snap(*pos_under_mouse, model.entities.get<footprint>(*ghost).dimensions));
            } else {
                auto map_entities = model.entities.view<te::site, te::pickable>();
                for (auto entity : map_entities) {
                    auto& map_site = map_entities.get<te::site>(entity);
                    if (glm::distance(map_site.position, *pos_under_mouse) <= 1.0f) {
                        entt_under_mouse = entity;
                        break;
                    }
                }
            }
        }
    }

    static auto prev_under_mouse = entt_under_mouse;

    if (prev_under_mouse != entt_under_mouse) {
        if (entt_under_mouse) {
            if (auto described = model.entities.try_get<te::described>(*entt_under_mouse); described) {
                //TODO: don't do this every frame
                //ui.ingame.bottom.text(described->description);
            } else {
                //ui.ingame.bottom.info.reset();
            }
        } else {
            //ui.ingame.bottom.info.reset();
        }
    }
    prev_under_mouse = entt_under_mouse;

    //TODO: move to ui somehow
    glm::vec3 forward = cam.forward();
    forward.z = 0.0f;
    forward = glm::normalize(forward);
    glm::vec3 left = glm::rotate(forward, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
    if (win.key(GLFW_KEY_W) == GLFW_PRESS) {
        cam.focus += 0.3f * forward;
    }
    if (win.key(GLFW_KEY_A) == GLFW_PRESS) cam.focus += 0.3f * left;
    if (win.key(GLFW_KEY_S) == GLFW_PRESS) cam.focus -= 0.3f * forward;
    if (win.key(GLFW_KEY_D) == GLFW_PRESS) cam.focus -= 0.3f * left;
    cam.use_ortho = win.key(GLFW_KEY_SPACE) != GLFW_PRESS;

    auto generator_wins = model.entities.view<std::shared_ptr<ui::generator_window>>();
    for (auto& e : generator_wins) {
        auto& win = generator_wins.get<std::shared_ptr<ui::generator_window>>(e);
        win->update(model, ui, e);
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
    ui.render(model);
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
