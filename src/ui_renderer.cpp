#include <te/ui_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <te/components.hpp>

constexpr glm::vec4 col_white = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr glm::vec4 col_red = glm::vec4{222, 93, 0, 255} / 255.0f;
constexpr glm::vec4 col_blue = glm::vec4{165, 219, 255, 255} / 255.0f;
constexpr glm::vec4 col_lblue = glm::vec4{57, 158, 222, 255} / 255.0f;

bool te::fontspec_comparator::operator()(const te::fontspec& lhs, const te::fontspec& rhs) const {
    return std::tie(lhs.filename, lhs.pts, lhs.aspect) < std::tie(rhs.filename, rhs.pts, rhs.aspect);
}

te::input_description te::ui_renderer::make_quad_input(gl::buffer<GL_ARRAY_BUFFER>& buf) {
    te::input_description inputs;
    GLsizei stride = sizeof(glm::vec2) + sizeof(glm::vec2) + sizeof(glm::vec4);
    inputs.elements = nullptr;
    inputs.attributes.emplace_back ( te::attribute_source {
        buf, te::gl::POSITION, 2, GL_FLOAT, GL_FALSE, stride, 0
    });
    inputs.attributes.emplace_back ( te::attribute_source {
        buf, te::gl::TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(glm::vec2))
    });
    inputs.attributes.emplace_back ( te::attribute_source {
        buf, te::gl::COLOUR, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(glm::vec4))
    });
    return inputs;
}

te::gl::texture2d te::ui_renderer::make_white1(te::gl::context& gl) {
    std::array<unsigned char, 4> white { 255, 255, 255, 255};
    te::gl::texture2d tex2d {gl.make_hnd<te::gl::texture_hnd>(glGenTextures), 1, 1};
    tex2d.bind();
    glTexImage2D (
        GL_TEXTURE_2D, 0, GL_RGBA,
        1, 1,
        0, GL_BGRA, GL_UNSIGNED_BYTE, white.data()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex2d;
}

te::ui_renderer::ui_renderer(te::window& wind):
    win { &wind },
    gl { &wind.gl },
    program { gl->link(gl->compile(te::file_contents("shaders/texquad_vertex.glsl"), GL_VERTEX_SHADER),
                       gl->compile(te::file_contents("shaders/texquad_fragment.glsl"), GL_FRAGMENT_SHADER),
                       te::gl::common_attribute_names) },
    projection { program.uniform("projection") },
    sampler_uform { program.uniform("tex") },
    sampler { gl->make_sampler() },
    white1 { make_white1(*gl) },
    quad_attributes { gl->make_buffer<GL_ARRAY_BUFFER>() },
    quad_input { make_quad_input(quad_attributes) },
    quad_vao { gl->make_vertex_array(quad_input) }
{
    sampler.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    sampler.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

#include <fmt/format.h>
ft::face& te::ui_renderer::face(te::fontspec key) {
    auto it = faces.find(key);
    if (it == faces.end()) {
        auto emplaced = faces.emplace (
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(ft.make_face(fmt::format("assets/fonts/{}", key.filename).c_str(), key.pts))
        );
        return emplaced.first->second;
    } else {
        return it->second;
    }
}

void te::ui_renderer::image(te::gl::texture2d& tex, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 src_pos, glm::vec2 src_size, glm::vec4 colour) {
    using namespace glm;
    quads.emplace_back (
        quad {
            vertex {dest_pos,                                src_pos + vec2{0.0f, 0.0f} * src_size, colour},
            vertex {dest_pos + vec2{0.0f, 1.0f} * dest_size, src_pos + vec2{0.0f, 1.0f} * src_size, colour},
            vertex {dest_pos + vec2{1.0f, 0.0f} * dest_size, src_pos + vec2{1.0f, 0.0f} * src_size, colour},
            vertex {dest_pos + vec2{1.0f, 1.0f} * dest_size, src_pos + vec2{1.0f, 1.0f} * src_size, colour}
        }
    );
    textures.emplace_back(&tex);
}

void te::ui_renderer::image(te::gl::texture2d& tex, glm::vec2 dest_pos, glm::vec2 src_pos, glm::vec2 src_size, glm::vec4 colour) {
    image(tex, dest_pos, glm::vec2{tex.width * src_size.x, tex.height * src_size.y}, src_pos, src_size, colour);
}

void te::ui_renderer::image(te::gl::texture2d& tex, glm::vec2 dest_pos, glm::vec4 colour) {
    image(tex, dest_pos, glm::vec2{tex.width, tex.height}, glm::vec2{0.0f, 0.0f}, glm::vec2{1.0f, 1.0f}, colour);
}

void te::ui_renderer::rect(glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec4 colour) {
    using namespace glm;
    quads.emplace_back (
        quad {
            vertex {dest_pos,                                vec2{0.0f, 0.0f}, colour},
            vertex {dest_pos + vec2{0.0f, 1.0f} * dest_size, vec2{0.0f, 1.0f}, colour},
            vertex {dest_pos + vec2{1.0f, 0.0f} * dest_size, vec2{1.0f, 0.0f}, colour},
            vertex {dest_pos + vec2{1.0f, 1.0f} * dest_size, vec2{1.0f, 1.0f}, colour}
        }
    );
    textures.emplace_back(&white1);
}

te::gl::texture2d& te::ui_renderer::glyph_texture(te::fontspec face_key, ft::glyph_index glyph_key) {
    auto& map = glyph_textures[face_key];
    auto glyph_it = map.find(glyph_key);
    if (glyph_it == map.end()) {
        FT_GlyphSlotRec glyph = face(face_key)[glyph_key];
        auto [emplaced_it, _] = map.emplace (
            std::piecewise_construct,
            std::forward_as_tuple(glyph_key),
            std::forward_as_tuple(gl->make_texture(glyph))
        );
        return emplaced_it->second;
    } else {
        return glyph_it->second;
    }
}

void te::ui_renderer::text(std::string_view str, glm::vec2 cursor, fontspec fref) {
    auto shaping_buffer = hb::buffer::shape(face(fref), str);
    unsigned int len = hb_buffer_get_length (shaping_buffer.hnd.get());
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos (shaping_buffer.hnd.get(), nullptr);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions (shaping_buffer.hnd.get(), nullptr);

    for (unsigned int i = 0; i < len; i++) {
        ft::glyph_index gix { info[i].codepoint };
        double x_offset = pos[i].x_offset / 64.0;
        double y_offset = pos[i].y_offset / 64.0;
        auto glyph = face(fref)[gix];
        image (
            glyph_texture(fref, gix),
            {
                cursor.x + x_offset + glyph.bitmap_left,
                cursor.y + y_offset - glyph.bitmap_top
            },
            fref.colour
        );
        cursor.x += pos[i].x_advance / 64.0;
        cursor.y += pos[i].y_advance / 64.0;
    }
}

void te::ui_renderer::render() {
    quad_attributes.bind();
    quad_attributes.upload(quads.begin(), quads.end(), GL_DYNAMIC_READ);
    glUseProgram(*program.hnd);
    glm::mat4 to_ndc = glm::ortho(0.0f, 1024.0f, 768.0f, 0.0f);
    glUniformMatrix4fv(projection, 1, GL_FALSE, glm::value_ptr(to_ndc));
    sampler.bind(0);
    quad_vao.bind();

    te::gl::texture2d* activated;
    for (auto i = 0u; i < quads.size(); i++) {
        if (textures[i] != activated) {
            textures[i]->activate(0);
            activated = textures[i];
        }
        glDrawArrays(GL_TRIANGLE_STRIP, i*4, 4);
    }
    quads.clear();
    textures.clear();
}

te::classic_ui::classic_ui(te::sim& s, te::window& w, te::ui_renderer& d, te::cache<asset_loader>& r):
    model{&s},
    input_win{&w},
    draw{&d},
    resources{&r},
    behind {
        .size = {w.width(), w.height()}
    } {
    show_window(std::make_unique<console>());
    thecon = reinterpret_cast<console*>(windows.back().get());
}

bool inside_rect(glm::vec2 pt, glm::vec2 tl, glm::vec2 br) {
    return pt.x >= tl.x && pt.x <= br.x && pt.y >= tl.y && pt.y <= br.y;
}

bool te::classic_ui::mouse_inside(glm::vec2 tl, glm::vec2 br) const {
    return cursor_pos.x >= tl.x && cursor_pos.x <= br.x
        && cursor_pos.y >= tl.y && cursor_pos.y <= br.y;
}

void te::classic_ui::input(glm::vec2 o, rect& r) {
    bool mouse_was_inside = inside_rect(prev_cursor_pos, o + r.offset, o + r.offset + r.size);
    bool mouse_is_inside = inside_rect(cursor_pos, o + r.offset, o + r.offset + r.size);
    if (mouse_was_inside && !mouse_is_inside && !leave_absorbed) {
        r.on_mouse_leave();
        leave_absorbed = true;
    }
    if (!mouse_was_inside && mouse_is_inside && !enter_absorbed) {
        r.on_mouse_enter();
        enter_absorbed = true;
    }
    if (mouse_was_inside && !mouse_was_down && mouse_is_inside && mouse_is_down && !down_absorbed) {
        r.click_started = true;
        down_absorbed = true;
        r.on_mouse_down();
    }
    if (mouse_was_inside && mouse_was_down && mouse_is_inside && !mouse_is_down) {
        if (r.click_started && !click_absorbed) {
            click_absorbed = true;
            r.on_click();
        }
        if (!up_absorbed) {
            r.on_mouse_up();
            up_absorbed = true;
        }
    }
    if (mouse_is_inside && !mouse_is_down) {
        r.click_started = false;
    }
}

void te::classic_ui::input(glm::vec2, label&) {
}

void te::classic_ui::drag_window::input(classic_ui& ui, glm::vec2 o) {
    ui.input(o, close);
    ui.input(o, frame);
}

void te::classic_ui::generator_window::input(classic_ui& ui, glm::vec2 o) {
    drag_window::input(ui, o);
}

bool te::classic_ui::input() {
    prev_cursor_pos = cursor_pos;
    mouse_was_down = mouse_is_down;

    glfwGetCursorPos(input_win->hnd.get(), &cursor_pos.x, &cursor_pos.y);
    mouse_is_down = glfwGetMouseButton(input_win->hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    enter_absorbed = false;
    leave_absorbed = false;
    click_absorbed = false;
    down_absorbed = false;
    up_absorbed = false;

    to_close = windows.end();
    to_bring_to_front = windows.end();

    for (auto it = windows.rbegin(); it != windows.rend(); it++) {
        (*it)->input(*this, (*it)->offset);
    }
    input({}, behind);

    if (to_close != windows.end()) {
        windows.erase(to_close);
    }

    if (to_bring_to_front != windows.end()) {
        std::rotate(to_bring_to_front, std::next(to_bring_to_front), windows.end());
    }

    if (dragging) {
        (*std::find_if(windows.begin(), windows.end(), [this](const auto& w){ return w->id == dragging; }))
            ->offset = glm::vec2{cursor_pos} + drag_offset;
    }

    return up_absorbed;
}

void te::classic_ui::draw_ui(glm::vec2 o, rect& r) {
    if (r.colour) {
        draw->rect(o + r.offset, r.size, *r.colour);
    }
    if (r.fname) {
        auto& tex = resources->lazy_load<te::gl::texture2d>(*r.fname);
        const glm::vec2 tex_size {tex.width, tex.height};
        draw->image(tex, o + r.offset, tex_size, {1.0, 1.0}, {1.0, 1.0});
    }
}

void te::classic_ui::draw_ui(glm::vec2 o, label& l) {
    draw->text(l.text, o + l.offset, l.font);
}

void te::classic_ui::draw_ui(glm::vec2 o, button& b) {
    auto& tex = resources->lazy_load<te::gl::texture2d>(*b.fname);
    const glm::vec2 tex_size {tex.width / 2.0, tex.height};
    const glm::vec2 tex_pos = b.click_started ? glm::vec2{0.5, 1.0} : glm::vec2{1.0, 1.0};
    draw->image(tex, o + b.offset, tex_size, tex_pos, {0.5, 1.0});
}

void te::classic_ui::drag_window::draw_ui(classic_ui& ui, glm::vec2 o) {
    ui.draw_ui(o, frame);
    ui.draw_ui(o, close);
}

void te::classic_ui::generator_window::draw_ui(classic_ui& ui, glm::vec2 o) {
    drag_window::draw_ui(ui, o);
    ui.draw_ui(o, title);
    ui.draw_ui(o, output_icon);
    ui.draw_ui(o, status);
    ui.draw_ui(o, output_name);
    ui.draw_ui(o, progress_backdrop);
    ui.draw_ui(o, progress);
}

te::classic_ui::generator_window::generator_window(te::sim& model, entt::entity e, const generator& generator) {
    frame = {
        .name = "frame",
        .fname = "assets/a_ui,6.{}/080.png",
        .size = {256.0f, 165.0f}
    };
    close = button {
        {
            .name = "close",
            .fname = "assets/a_ui,6.{}/041.png",
            .offset = glm::vec2{226, 12},
            .size = {28, 27}
        }
    };
    title = label {
        .offset = {16, 28},
        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Regular.ttf", 8.0, 1.0, col_white},
        .text = model.entities.get<te::named>(e).name
    };
    inspected = e;
    status = label {
        .offset = {39, 73},
        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7, 1.0, col_red},
        .text = "Shut Down"
    };
    output_name = label {
        .offset = {70, 101},
        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7, 1.0, {255/255.0, 195/255.0, 66/255.0, 255/255.0}},
        .text = model.entities.get<te::named>(generator.output).name
    };
    output_icon = rect {
        .fname = model.entities.try_get<te::render_tex>(generator.output)->filename,
        .offset = {38, 85},
        .size = {21, 21}
    };
    progress_backdrop = rect {
        .fname = "assets/a_ui,6.{}/116.png",
        .offset = {33, 122}
    };
    progress = rect {
        .colour = col_lblue,
        .offset = {34, 123}
    };
}

constexpr glm::vec4 col_yellow = glm::vec4{100.0, 76.5, 25.9, 100} / 100.0f;
constexpr glm::vec4 col_grey = glm::vec4{87.1, 78.0, 64.7, 100} / 100.0f;
te::classic_ui::market_window::market_window(te::sim& model, entt::entity e, market&) : inspected{e} {
    frame = rect {
        .fname = "assets/a_ui,6.{}/104.png",
        .size = {504, 431}
    };
    close = button {
        {
            .fname = "assets/a_ui,6.{}/041.png",
            .offset = {471, 21},
            .size = {28, 27}
        }
    };
    title = label {
        .offset = {93, 32},
        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 8.0, 1.0, col_yellow},
        .text = "Eastern Seila" //model.entities.get<te::named>(e).name
    };
    founded = label {
        .offset = {44, 46},
        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Regular.ttf", 6.0, 1.0, col_grey},
        .text = "Founded 2000BC by Memphis Family"
    };
}

void te::classic_ui::market_window::draw_ui(classic_ui& ui, glm::vec2 o) {
    drag_window::draw_ui(ui, o);
    ui.draw_ui(o, title);
    ui.draw_ui(o, founded);
}

void te::classic_ui::market_window::input(classic_ui& ui, glm::vec2 o) {
    drag_window::input(ui, o);
}
void te::classic_ui::market_window::update(te::sim&) {
}

void te::classic_ui::generator_window::update(te::sim& model) {
    auto generator = model.entities.try_get<te::generator>(inspected);
    progress.size = {189.0 * generator->progress, 8};
}

void te::classic_ui::show_window(std::unique_ptr<drag_window> w) {
    auto& appended = windows.emplace_back(std::move(w));
    unsigned win_id = appended->id = next_id++;
    appended->close.on_click.connect([this, win_id]() {
        to_close = std::find_if(windows.begin(), windows.end(), [win_id](const auto& w) { return w->id == win_id; });
    });
    appended->frame.on_mouse_down.connect([this, win_id]() {
        dragging = win_id;
        auto me = std::find_if(windows.begin(), windows.end(), [win_id](const auto& w) { return w->id == win_id; });
        to_bring_to_front = me;
        drag_offset = (*me)->offset - glm::vec2{cursor_pos};
        (*me)->frame.on_mouse_up.connect_extended([this](const boost::signals2::connection& up_conn) {
            dragging.reset();
            up_conn.disconnect();
        });
    });
}

void te::classic_ui::inspect(entt::entity inspected, te::generator& generator) {
    auto it = std::find_if (
        windows.begin(),
        windows.end(),
        [inspected](const std::unique_ptr<drag_window>& w) {
            if (generator_window* gw = dynamic_cast<generator_window*>(w.get()); gw != nullptr) {
                return gw->inspected == inspected;
            }
            return false;
        }
    );
    if (it == windows.end()) {
        show_window(std::make_unique<generator_window>(*model, inspected, generator));
    } else {
        std::rotate(it, std::next(it), windows.end());
    }
}

void te::classic_ui::inspect(entt::entity inspected, te::market& market) {
    auto it = std::find_if (
        windows.begin(),
        windows.end(),
        [inspected](const std::unique_ptr<drag_window>& w) {
            if (market_window* mw = dynamic_cast<market_window*>(w.get()); mw != nullptr) {
                return mw->inspected == inspected;
            }
            return false;
        }
    );
    if (it != windows.end()) {
        show_window(std::make_unique<market_window>(*model, inspected, market));
    } else {
        std::rotate(it, std::next(it), windows.end());
    }
}

void te::classic_ui::render() {
    for (auto& win : windows) {
        win->update(*model);
        win->draw_ui(*this, win->offset);
    }
    draw->render();
}

te::classic_ui::console::console() {
    offset = {20, 20};
    frame = rect {
        .colour = glm::vec4{0, 0, 0, 1},
        .size {300, 300}
    };
    close = button {
        {
            .name = "close",
            .fname = "assets/a_ui,6.{}/041.png",
            .offset = glm::vec2{260, 10},
            .size = {28, 27}
        }
    };
}

void te::classic_ui::console::input(te::classic_ui& ui, glm::vec2 o) {
    drag_window::input(ui, o);
}

void te::classic_ui::console::update(te::sim&) {
}

const auto console_font = te::fontspec {
    .filename = "NotoMono-Regular.ttf",
    .pts = 8,
    .aspect = .8,
    .colour = glm::vec4{1.0}
};

void te::classic_ui::console::draw_ui(te::classic_ui& ui, glm::vec2 o) {
    drag_window::draw_ui(ui, o);
    ui.draw->text(line, o + glm::vec2{1.0, 24.0}, console_font);
}

void te::classic_ui::on_char(unsigned int code) {
    spdlog::debug(code);
    thecon->line += code;
}
