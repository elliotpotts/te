#include <te/ui_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <te/components.hpp>

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
        .name = "behind",
        .size = {w.width(), w.height()}
    } {
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
        spdlog::debug("mouse leave #{}", r.name);
        r.on_mouse_leave();
        leave_absorbed = true;
    }
    if (!mouse_was_inside && mouse_is_inside && !enter_absorbed) {
        spdlog::debug("mouse enter #{}", r.name);
        r.on_mouse_enter();
        enter_absorbed = true;
    }
    if (mouse_was_inside && !mouse_was_down && mouse_is_inside && mouse_is_down && !down_absorbed) {
        spdlog::debug("mouse down #{}", r.name);
        r.click_started = true;
        down_absorbed = true;
        r.on_mouse_down();
    }
    if (mouse_was_inside && mouse_was_down && mouse_is_inside && !mouse_is_down) {
        if (r.click_started && !click_absorbed) {
            spdlog::debug("mouse click #{}", r.name);
            click_absorbed = true;
            r.on_click();
        }
        if (!up_absorbed) {
            spdlog::debug("mouse up #{}", r.name);
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

void te::classic_ui::input(glm::vec2 o, drag_window& w) {
    input(o, w.close);
    input(o, w.frame);
}

void te::classic_ui::input(glm::vec2 o, generator_window& w) {
    input(o, static_cast<drag_window&>(w));
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
        input(it->offset, *it);
    }
    input({}, behind);

    if (to_close != windows.end()) {
        windows.erase(to_close);
    }

    if (to_bring_to_front != windows.end()) {
        std::rotate(to_bring_to_front, std::next(to_bring_to_front), windows.end());
    }

    if (dragging) {
        std::find_if(windows.begin(), windows.end(), [this](const auto& w){ return w.id == dragging; })
            ->offset = glm::vec2{cursor_pos} + drag_offset;
    }

    return up_absorbed;
}

void te::classic_ui::draw_ui(glm::vec2 o, rect& r) {
    if (!r.fname.empty()) {
        auto& tex = resources->lazy_load<te::gl::texture2d>(r.fname);
        const glm::vec2 tex_size {tex.width, tex.height};
        draw->image(tex, o + r.offset, tex_size, {1.0, 1.0}, {1.0, 1.0});
    }
}

void te::classic_ui::draw_ui(glm::vec2 o, label& l) {
    draw->text(l.text, o + l.offset, l.font);
}

void te::classic_ui::draw_ui(glm::vec2 o, button& b) {
    auto& tex = resources->lazy_load<te::gl::texture2d>(b.fname);
    const glm::vec2 tex_size {tex.width / 2.0, tex.height};
    const glm::vec2 tex_pos = b.click_started ? glm::vec2{0.5, 1.0} : glm::vec2{1.0, 1.0};
    draw->image(tex, o + b.offset, tex_size, tex_pos, {0.5, 1.0});
}

void te::classic_ui::draw_ui(glm::vec2 o, drag_window& w) {
    draw_ui(o, w.frame);
    draw_ui(o, w.title);
    draw_ui(o, w.close);
}

void te::classic_ui::draw_ui(glm::vec2 o, generator_window& w) {
    draw_ui(o, static_cast<drag_window&>(w));
    auto generator = model->entities.try_get<te::generator>(w.inspected);
    draw_ui(o, w.output_icon);
    draw_ui(o, w.status);
    draw_ui(o, w.output_name);
    // Output progress
    draw->image(resources->lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/116.png"), o + glm::vec2{33, 122});
    draw->rect(o + glm::vec2{34, 123}, {189.0 * generator->progress, 8}, {57/255.0, 158/255.0, 222/255.0, 255/255.0});
}

void te::classic_ui::open_generator(entt::entity inspected) {
    auto it = std::find_if (
        windows.begin(),
        windows.end(),
        [inspected](generator_window& w) {
            return w.inspected == inspected;
        }
    );
    static unsigned next_id = 0;
    if (it == windows.end()) {
        const auto& frame_tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}.png", "080"));
        const auto& button_tex = resources->lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/041.png");
        const glm::vec2 button_size { button_tex.width, button_tex.height };
        const auto generator = model->entities.try_get<te::generator>(inspected);
        const auto output_rtex = model->entities.try_get<te::render_tex>(generator->output);
        const auto& output_tex = resources->lazy_load<te::gl::texture2d>(output_rtex->filename);
        windows.push_back (
            generator_window {
                /*.drag =*/ drag_window {
                    .id = next_id++,
                    .frame = {
                        .name = "frame",
                        .fname = "assets/a_ui,6.{}/080.png",
                        .size = {256.0f, 165.0f}
                    },
                    .close = button {
                        /*.rect = */ {
                            .name = "close",
                            .fname = "assets/a_ui,6.{}/041.png",
                            .offset = glm::vec2{226, 12},
                            .size = button_size
                        }
                    },
                    .title = label {
                        .offset = {16, 28},
                        .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Regular.ttf", 8.0, 1.0, col_white},
                        .text = model->entities.get<te::named>(inspected).name
                    }
                },
                /*.inspected =*/ inspected,
                /*.status =*/ label {
                    .offset = {39, 73},
                    .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7, 1.0, {165/255.0, 219/255.0, 255/255.0, 255/255.0}},
                    .text = "Producing"
                },
                /*.output_name=*/ label {
                    .offset = {70, 101},
                    .font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7, 1.0, {255/255.0, 195/255.0, 66/255.0, 255/255.0}},
                    .text = model->entities.get<te::named>(generator->output).name
                },
                /*.output_icon=*/ rect {
                    .fname = model->entities.try_get<te::render_tex>(generator->output)->filename,
                    .offset = {38, 85},
                    .size = { output_tex.width, output_tex.height }
                }
            }
        );
        auto& appended = windows.back();
        auto my_id = appended.id;
        appended.close.on_click.connect([this, my_id]() {
            to_close = std::find_if(windows.begin(), windows.end(), [my_id](const auto& w){ return w.id == my_id; });
        });
        appended.frame.on_mouse_down.connect([this, my_id]() {
            dragging = my_id;
            auto me = std::find_if(windows.begin(), windows.end(), [my_id](const auto& w){ return w.id == my_id; });
            to_bring_to_front = me;
            drag_offset = me->offset - glm::vec2{cursor_pos};
            me->frame.on_mouse_up.connect_extended([this](const boost::signals2::connection& up_conn) {
                dragging.reset();
                up_conn.disconnect();
            });
        });
    } else {
        std::rotate(it, std::next(it), windows.end());
    }
}

void te::classic_ui::render() {
    for (auto& win : windows) {
        draw_ui(win.offset, win);
    }
    draw->render();
}
