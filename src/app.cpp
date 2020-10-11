#include <te/app.hpp>
#include <te/util.hpp>
#include <te/server.hpp>
#include <te/client.hpp>
#include <te/net.hpp>
#include <fmt/format.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>
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

namespace {
    double fps = 0.0;
    ImGuiIO& setup_imgui(te::window& win) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplGlfw_InitForOpenGL(win.hnd.get(), true);
        ImGui_ImplOpenGL3_Init("#version 130");
        //io.Fonts->AddFontFromFileTTF("assets/LiberationSans-Regular.ttf", 18);
        //io.Fonts->AddFontFromFileTTF("assets/LiberationMono-Regular.ttf", 18);
        return io;
    }
}

namespace {
    const auto light_blue = ImColor(ImVec4{22.9/100.0, 60.7/100.0, 85.9/100.0, 1.0});
    const auto dark_blue = ImColor(ImVec4{22.9/255.0, 60.7/255.0, 85.9/255.0, 1.0f});
    const auto white = ImColor(ImVec4{1.0, 1.0, 1.0, 1.0});
    const auto red = ImColor(ImVec4{1.0, 0.0, 0.0, 1.0});
}

te::app::app(te::sim& model, SteamNetworkingIPAddr server_addr) :
    rengine { 42 },
    win { glfw.make_window(1024, 768, "Trade Empires", false)},
    fmod { te::make_fmod_system() },
    imgui_io { setup_imgui(win) },
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
    ui { model, win, ui_renderer, resources }
{
    win.set_cursor(make_bitmap("assets/ui/cursor.png"));
    fmod->createStream("assets/music/main-theme.ogg", FMOD_CREATESTREAM | FMOD_LOOP_NORMAL, nullptr, &menu_music_src);
    //fmod->playSound(menu_music_src, nullptr, false, &menu_music);
    win.on_framebuffer_size.connect([&](int width, int height) {
        cam.aspect_ratio = static_cast<float>(width) / height;
        glViewport(0, 0, width, height);
    });
    win.set_attribute(GLFW_RESIZABLE, GLFW_TRUE);
    win.on_mouse_button.connect([&](int button, int action, int mods) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            on_mouse_button(button, action, mods);
        }
    });
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
    console.emplace_back(console_line{ImColor{255, 255, 255}, fmt::format("[{}]: ", msg.from), msg.content});
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

void te::app::imgui_commicon(entt::entity e, ImVec4 colour) {
    auto& render_tex = model.entities.get<te::render_tex>(e);
    ImGui::Image (
        *resources.lazy_load<te::gl::texture2d>(render_tex.filename).hnd,
        ImVec2{21, 21}, ImVec2{0, 0}, ImVec2{1, 1}, ImVec4{1, 1, 1, 1},
        colour
    );
}

void te::app::render_mappos_inspector(const te::site& site, const te::named& named) {
    ImGui::Text("Map position: (%f, %f)", site.position.x, site.position.y);
    auto id_string = fmt::format("{}", *reinterpret_cast<std::uint32_t*>(&*inspected));
    ImGui::Text("%s", named.name.c_str());
    ImGui::Separator();
}

void te::app::render_generator_inspector(const te::generator& generator) {
    imgui_commicon(generator.output);
    ImGui::SameLine();
    const auto& output_commodity_name = model.entities.get<te::named>(generator.output);
    ImGui::Text(fmt::format("{} @ {}/s", output_commodity_name.name, generator.rate).c_str());
    ImGui::SameLine();
    ImGui::ProgressBar(generator.progress);
    ImGui::Separator();
}

void te::app::render_demander_inspector(const te::demander& demander) {
    for (auto [demanded, rate] : demander.rate) {
        imgui_commicon(demanded);
        ImGui::SameLine();
        ImGui::Text(fmt::format("{} @ {}/s", model.entities.get<te::named>(demanded).name, rate).c_str());
    }
    ImGui::Separator();
}

void te::app::render_inventory_inspector(const te::inventory& inventory) {
    for (auto [commodity_entity, stock] : inventory.stock) {
        auto& name = model.entities.get<te::named>(commodity_entity);
        ImGui::Text(fmt::format("{}x {}", stock, name.name).c_str());
    }
    ImGui::Separator();
}

void te::app::render_trader_inspector(const te::trader& trader) {
    ImGui::Text(fmt::format("Running balance: {}", trader.balance).c_str());
    ImGui::Separator();
}

void te::app::render_producer_inspector(const te::producer& producer, const te::inventory& inventory) {
    ImGui::Text("Inputs");
    for (auto& [commodity_e, needed] : producer.inputs) {
        imgui_commicon(commodity_e);
        ImGui::SameLine();
        auto commodity_stock_it = inventory.stock.find(commodity_e);
        int commodity_stock = commodity_stock_it == inventory.stock.end() ? 0 : commodity_stock_it->second;
        ImGui::Text(fmt::format("{}: {}/{}", model.entities.get<named>(commodity_e).name, commodity_stock, needed).c_str());
    }
    ImGui::ProgressBar(producer.progress);
    ImGui::Text(fmt::format("Outputs @{}/s", producer.rate).c_str());
    for (auto& [commodity_e, produced] : producer.outputs) {
        imgui_commicon(commodity_e);
        ImGui::SameLine();
        ImGui::Text(fmt::format("{}: ×{}", model.entities.get<named>(commodity_e).name, produced).c_str());
    }
}

void te::app::render_market_inspector(const te::market& market) {
    ImGui::Text("Trading at this market:");
    ImGui::Columns(4);

    ImGui::Text("Party");
    ImGui::NextColumn();

    ImGui::Text("Bid");
    ImGui::NextColumn();

    ImGui::NextColumn();

    ImGui::Text("Commodity");
    ImGui::NextColumn();


    for (auto entity : market.trading) {
        if (auto [named, trader] = model.entities.try_get<te::named, te::trader>(entity); named && trader) {
            bool any_bids = std::any_of (
                trader->bid.begin(),
                trader->bid.end(),
                [](auto pair) {
                    auto [commodity_e, bid] = pair;
                    return bid != 0.0;
                }
            );
            any_bids = true;
            if (!any_bids) {
                continue;
            }
            ImGui::Text(fmt::format("* {}", named->name).c_str());
            for (auto [commodity_e, bid] : trader->bid) {
                if (bid == 0.0) {
                    continue;
                }
                ImGui::NextColumn();

                if (bid > 0.0) {
                    ImGui::Text("Buy");
                    ImGui::NextColumn();
                    ImGui::TextColored(light_blue, fmt::format("{:.1}", bid).c_str());
                } else if (bid < 0.0) {
                    ImGui::Text("Sell");
                    ImGui::NextColumn();
                    ImGui::TextColored(red, fmt::format("{:.1}", std::abs(bid)).c_str());
                }
                ImGui::NextColumn();

                imgui_commicon(commodity_e);
                ImGui::SameLine();
                ImGui::Text(model.entities.get<te::named>(commodity_e).name.c_str());
                ImGui::NextColumn();
            }
        }
    }
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::Text(fmt::format("Population: {}", market.population).c_str());
    ImGui::Text(fmt::format("Growth rate: {}", market.growth_rate).c_str());
    ImGui::Text("Growth: ");
    ImGui::SameLine();
    float fake_growth_rate = market.growth_rate;
    ImGui::SliderFloat("", &fake_growth_rate, -1.0f / 3.0, 1.0f / 4.0);
    ImGui::ProgressBar((glm::clamp(market.growth, -1.0, 1.0) + 1.0) / 2.0);

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
    for (auto [commodity_entity, price] : market.prices) {
        auto& commodity_name = model.entities.get<te::named>(commodity_entity);
        ImGui::Text(fmt::format("{}", model.market_stock(*inspected, commodity_entity)).c_str());
        ImGui::NextColumn();

        imgui_commicon(commodity_entity);
        ImGui::NextColumn();

        ImGui::Text(commodity_name.name.c_str());
        ImGui::NextColumn();

        auto commodity_demand_it = market.demand.find(commodity_entity);
        double commodity_demand = commodity_demand_it == market.demand.end() ? 0.0 : commodity_demand_it->second;
        auto commodity_price_it = market.prices.find(commodity_entity);
        double commodity_price = commodity_price_it == market.prices.end() ? 0.0 : commodity_price_it->second;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor_pos {ImGui::GetCursorScreenPos()};
        const float bar_bottom = cursor_pos.y + 20.0f;
        const float bar_height = 10.0f;
        const float bar_top = bar_bottom - bar_height;
        const float bar_left = cursor_pos.x;
        const float bar_width = width_available - 16;
        const float bar_right = cursor_pos.x + bar_width;
        const float bar_centre = (bar_left + bar_right) / 2.0;
        draw->AddRectFilled (
            ImVec2{bar_left, bar_top},
            ImVec2{bar_left + bar_width * std::min(static_cast<float>(commodity_demand), 1.0f), bar_bottom},
            light_blue, 0, 0.0f
        );
        draw->AddRectFilled (
            ImVec2{bar_right - bar_width * (std::max(1.0f - static_cast<float>(commodity_demand), 0.0f)), bar_top},
            ImVec2{bar_right, bar_bottom},
            dark_blue, 0, 0.0f
        );

        std::string price_string = fmt::format("{:.1f}", commodity_price);
        const char* price_string_begin = std::to_address(price_string.begin());
        const char* price_string_end = std::to_address(price_string.end());
        ImVec2 price_text_size = ImGui::CalcTextSize(price_string_begin, price_string_end);
        const float text_left = bar_centre - price_text_size.x / 2.0f;
        draw->AddText(ImVec2{text_left, cursor_pos.y}, white, price_string_begin, price_string_end);

        int marker_count = static_cast<int>(commodity_demand);
        float gap_size = ((std::floor(commodity_demand) / commodity_demand) * bar_width) / static_cast<float>(marker_count);
        for (int i = 0; i < marker_count; i++) {
            draw->AddRectFilled (
                ImVec2{bar_left + (i + 1) * gap_size - 2.0f, bar_top - 1.0f},
                ImVec2{bar_left + (i + 1) * gap_size + 2.0f, bar_bottom + 1.0f},
                white, 0, 0.0f
            );
        }
        ImGui::NextColumn();

        ImGui::Text(fmt::format("{}x", static_cast<int>(commodity_demand)).c_str());
        ImGui::NextColumn();
    }
    ImGui::Separator();
    ImGui::Columns();
}

void te::app::render_inspectors() {
    ImGui::Begin("Inspector", nullptr, 0);
    ImGui::Text("FPS: %f", fps);
    ImGui::Separator();
    if (inspected) {
        if (auto [maybe_site, maybe_named] = model.entities.try_get<te::site, te::named>(*inspected); maybe_site && maybe_named) {
            render_mappos_inspector(*maybe_site, *maybe_named);
        }
        if (auto generator = model.entities.try_get<te::generator>(*inspected); generator) {
            render_generator_inspector(*generator);
        }
        if (auto demander = model.entities.try_get<te::demander>(*inspected); demander) {
            render_demander_inspector(*demander);
        }
        if (auto inventory = model.entities.try_get<te::inventory>(*inspected); inventory && !inventory->stock.empty()) {
            render_inventory_inspector(*inventory);
        }
        if (auto trader = model.entities.try_get<te::trader>(*inspected); trader) {
            render_trader_inspector(*trader);
        }
        if (auto [producer, inventory] = model.entities.try_get<te::producer, te::inventory>(*inspected); producer && inventory) {
            render_producer_inspector(*producer, *inventory);
        }
        if (auto market = model.entities.try_get<te::market>(*inspected); market) {
            render_market_inspector(*market);
        }
    }
    ImGui::End();
}

void te::app::render_console() {
    ImGui::Begin("Console", nullptr, 0);
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // 1 separator, 1 input text
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar); // Leave room for 1 separator + 1 InputText
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("Clear")) console.clear();
        ImGui::EndPopup();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1));
    for (auto& line : console) {
        ImGui::PushStyleColor(ImGuiCol_Text, static_cast<ImVec4>(light_blue));
        ImGui::TextUnformatted(line.prefix.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted(line.content.c_str());
    }
    if (scroll_console_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        scroll_console_to_bottom = false;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();
    static std::array<char, 200> line_buf {'\0'};
    if (ImGui::InputText("Input", line_buf.data(), line_buf.size(), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr) && client && client->nick()) {
        te::chat msg;
        msg.from = *client->nick();
        msg.content = std::string{line_buf.data(), line_buf.size()};
        client->send(msg);
        line_buf.fill('\0');
        ImGui::SetKeyboardFocusHere(-1);
        scroll_console_to_bottom = true;
    }
    ImGui::End();
}

void te::app::render_merchant_summary(entt::entity merchant_entity) {
    const auto& merchant = model.entities.get<te::merchant>(merchant_entity);
    const auto& merchant_inventory = model.entities.get<te::inventory>(merchant_entity);
    const auto& merchant_name = model.entities.get<te::named>(merchant_entity);
    const auto& trader = model.entities.get<te::trader>(merchant_entity);
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 66.0);
    const auto& mule_tex = resources.lazy_load<te::gl::texture2d>("assets/yellowmule.png");
    ImGui::Image(*mule_tex.hnd, ImVec2{64, 54});
    ImGui::NextColumn();
    if (ImGui::Button(merchant_name.name.c_str())) {
        merchant_ordering = merchant_entity;
    }
    ImGui::SameLine();
    ImGui::Text(fmt::format("¤{}", trader.balance).c_str());
    auto status = model.merchant_status(merchant_entity);
    if (status) {
        ImGui::Text(merchant.route->name.c_str());
        if (status->trading) {
            ImGui::Text(fmt::format("Trading at {}", model.entities.get<te::named>(status->next.where).name).c_str());
        } else {
            ImGui::Text(fmt::format("En route to {}", model.entities.get<te::named>(status->next.where).name).c_str());
        }
        ImGui::NextColumn();
        ImGui::Columns();
        ImGui::NewLine();
        for (auto commodity : model.commodities) {
            const auto stock_it = merchant_inventory.stock.find(commodity);
            const int stock = stock_it == merchant_inventory.stock.end() ? 0 : stock_it->second;
            const auto leave_with_it = status->next.leave_with.find(commodity);
            const int leave_with = leave_with_it == status->next.leave_with.end() ? 0 : leave_with_it->second;
            const int buy = std::max(0, leave_with - stock);
            const int sell = status->trading ? std::max(0, stock - leave_with) : 0;
            const int keep = stock - sell;
            for (int i = 0; i < buy; i++) {
                ImGui::SameLine();
                imgui_commicon(commodity, light_blue);
            }
            for (int i = 0; i < sell; i++) {
                ImGui::SameLine();
                imgui_commicon(commodity, red);
            }
            for (int i = 0; i < keep; i++) {
                ImGui::SameLine();
                imgui_commicon(commodity);
            }
        }
    } else {
        ImGui::Text("No route assigned");
        ImGui::NextColumn();
        ImGui::Columns();
        for (auto [commodity, stock] : merchant_inventory.stock) {
            const auto& commodity_tex = resources.lazy_load<te::gl::texture2d>(model.entities.get<te::render_tex>(commodity).filename);
            for (int i = 0; i < stock; i++) {
                ImGui::SameLine();
                ImGui::Image(*commodity_tex.hnd, ImVec2{24, 24}, ImVec2{0, 0}, ImVec2{1, 1}, ImVec4{1, 1, 1, 1}, ImVec4{1, 1, 1, 1});
            }
        }
    }
    ImGui::Separator();
}

void te::app::render_orders_controller() {
    render_merchant_summary(*merchant_ordering);

    static std::optional<int> selected_route_ix = std::nullopt;
    if (ImGui::ListBoxHeader("###route_selector")) {
        for (std::size_t i = 0; i < model.routes.size(); i++) {
            bool current_item_selected = i == selected_route_ix;
            if (ImGui::Selectable(model.routes[i].name.c_str(), current_item_selected)) {
                selected_route_ix = i;
            }
            if (current_item_selected) {
                ImGui::SetItemDefaultFocus();
            }
        };
        ImGui::ListBoxFooter();
    }

    if (ImGui::Button("Embark on route") && selected_route_ix) {
        playsfx("assets/sfx/route4.wav");
        model.merchant_embark(*merchant_ordering, model.routes[*selected_route_ix]);
    }

    if (ImGui::Button("Clear orders")) {
        playsfx("assets/sfx/route3.wav");
    }

    if (ImGui::Button("Show merchant roster")) {
        merchant_ordering.reset();
    }
}

void te::app::render_roster_controller() {
    if (client && client->family()) {
        unsigned my_family = *client->family();
        auto merchants = model.entities.view<te::trader, te::merchant, te::inventory, te::named, te::owned>();
        for (auto merchant_entity : merchants) {
            if (model.entities.get<te::owned>(merchant_entity).family_ix == my_family) {
                render_merchant_summary(merchant_entity);
            }
        }
    }
    static const std::vector<std::string> merch_names = { "Zazoo", "Mufasa", "Rafiki" };
    static auto merch_name = merch_names.begin();
    if (ImGui::Button("Hire new merchant: ¤600") && merch_name != merch_names.end()) {
        playsfx("assets/sfx/misc5.wav");
        //TODO: move to server
        auto merchant_e = model.entities.create();
        model.entities.emplace<te::named>(merchant_e, *merch_name++);
        model.entities.emplace<te::owned>(merchant_e, *client->family());
        model.entities.emplace<te::site>(merchant_e, glm::vec2{0.0f, 0.0f});
        model.entities.emplace<te::footprint>(merchant_e, glm::vec2{1.0f, 1.0f});
        model.entities.emplace<te::render_mesh>(merchant_e, "assets/merchant.glb");
        model.entities.emplace<te::pickable>(merchant_e);
        model.entities.emplace<te::trader>(merchant_e, 1u);
        model.entities.emplace<te::inventory>(merchant_e);
        model.entities.emplace<te::merchant>(merchant_e, std::nullopt);
    }
}

void te::app::render_merchants_controller() {
    if (merchant_ordering) {
        render_orders_controller();
    } else {
        render_roster_controller();
    }
    ImGui::EndTabItem();
}

void te::app::render_routes_controller() {
    static std::optional<int> selected_route_ix = std::nullopt;
    if (ImGui::BeginCombo("###route_selector", selected_route_ix ? model.routes[*selected_route_ix].name.c_str() : "")) {
        for (std::size_t i = 0; i < model.routes.size(); i++) {
            bool current_item_selected = i == selected_route_ix;
            if (ImGui::Selectable(model.routes[i].name.c_str(), current_item_selected)) {
                selected_route_ix = i;
            }
            if (current_item_selected) {
                ImGui::SetItemDefaultFocus();
            }
        };
        ImGui::EndCombo();
    }
    if (ImGui::Button("New")) {
        playsfx("assets/sfx/misc5.wav");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && selected_route_ix) {
        playsfx("assets/sfx/route5.wav");
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        playsfx("assets/sfx/misc5.wav");
    }
    ImGui::Separator();
    if (selected_route_ix) {
        for (const te::stop& stop : model.routes[*selected_route_ix].stops) {
            ImGui::Text(model.entities.get<te::named>(stop.where).name.c_str());
            for (auto [commodity, leave_with] : stop.leave_with) {
                const auto& commodity_tex = resources.lazy_load<te::gl::texture2d>(model.entities.get<te::render_tex>(commodity).filename);
                for (int i = 0; i < leave_with; i++) {
                    ImGui::SameLine();
                    ImGui::Image(*commodity_tex.hnd, ImVec2{24, 24}, ImVec2{0, 0}, ImVec2{1, 1}, ImVec4{1, 1, 1, 1}, ImVec4{1, 1, 1, 1});
                }
            }
            ImGui::Separator();
        }
    }
    auto markets = model.entities.view<te::market, te::named, te::site>();
    auto market_it = markets.begin();
    static std::optional<decltype(market_it)> selected_next_stop_it = std::nullopt;
    const char* const combo_preview = selected_next_stop_it ? model.entities.get<te::named>(**selected_next_stop_it).name.c_str() : "";
    if (ImGui::BeginCombo("###next_stop_selector", combo_preview)) {
        for (;market_it != markets.end(); market_it++) {
            bool current_next_stop_selected = market_it == selected_next_stop_it;
            auto name = model.entities.get<te::named>(*market_it).name;
            if (ImGui::Selectable(name.c_str(), current_next_stop_selected)) {
                selected_next_stop_it = market_it;
            }
            if (current_next_stop_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Stop") && selected_route_ix && selected_next_stop_it) {
        playsfx("assets/sfx/misc5.wav");
        model.routes[*selected_route_ix].stops.push_back(stop {**selected_next_stop_it, {}});
    }
    ImGui::Separator();
    for (auto commodity : model.commodities) {
        const auto& commodity_tex = resources.lazy_load<te::gl::texture2d>(model.entities.get<te::render_tex>(commodity).filename);
        if (ImGui::ImageButton(*commodity_tex.hnd, ImVec2{24, 24}) && selected_route_ix && model.routes[*selected_route_ix].stops.size() > 0) {
            model.routes[*selected_route_ix].stops.back().leave_with[commodity]++;
            playsfx("assets/sfx/route1.wav");
            ImGui::Text("One more!");
        }
        ImGui::SameLine();
        ImGui::Text(model.entities.get<te::named>(commodity).name.c_str());
    }
    ImGui::EndTabItem();
}

void te::app::render_construction_controller() {
    for (auto blueprint : model.blueprints) {
        if (auto [named, price, footprint] = model.entities.try_get<te::named, te::price, te::footprint>(blueprint); named && price && footprint) {
            if (ImGui::Button(fmt::format("{}: ¤{}", named->name, price->price).c_str()) && !ghost) {
                playsfx("assets/sfx/misc5.wav");
                ghost = model.entities.create();
                model.entities.emplace<te::footprint>(*ghost, model.entities.get<te::footprint>(blueprint));
                model.entities.emplace<te::render_mesh>(*ghost, model.entities.get<te::render_mesh>(blueprint));
                model.entities.emplace<te::ghost>(*ghost, blueprint);
            }
        }
    }
    if (ghost && ImGui::Button("Cancel construction")) {
        playsfx("assets/sfx/misc5.wav");
        model.entities.destroy(*ghost);
        ghost.reset();
    }
    ImGui::EndTabItem();
}

void te::app::render_technology_controller() {
    ImGui::EndTabItem();
}

void te::app::render_controller() {
    ImGui::Begin("Controller", nullptr, 0);
    ImGui::Text(fmt::format("¤{}", client && client->family() ? model.families[*client->family()].balance : -1).c_str());
    if (ImGui::BeginTabBar("MainTabbar")) {
        if (ImGui::BeginTabItem("Merchants")) render_merchants_controller();
        if (ImGui::BeginTabItem("Routes")) render_routes_controller();
        if (ImGui::BeginTabItem("Build")) render_construction_controller();
        if (ImGui::BeginTabItem("Technology")) render_technology_controller();
        ImGui::EndTabBar();
    }
    ImGui::End();
}

static bool at_main_menu = false;
bool te::app::render_main_menu() {
    /*
    ui.texquad(resources.lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/124.png"), {0, 0}, {1024, 768});
    glm::vec2 cursor {556, 160};
    static std::vector<std::string_view> strs {
        "New Game", "Load Game", "Host Server", "Connect to Server", "Quit"
    };
    for (auto s : strs) {
        if (ui.button(s, cursor, 30.0)) {
            at_main_menu = false;
        }
        cursor.y += 60.0;
    }
    return false;
    */
}

struct panel {
    std::string_view name;
    int pane;
    int icon;
};

void te::app::render_ui() {
    //render_console();
    if (at_main_menu) {
        render_main_menu();
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

    if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput) {
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
