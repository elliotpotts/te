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

void te::ui_renderer::text(std::string_view str, glm::vec2 cursor, fontspec fref, glm::vec4 colour) {
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
            colour
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

te::classic_ui::classic_ui(te::sim& s, te::window& w, te::ui_renderer& d, te::cache<asset_loader>& r): model(&s), input_win(&w), draw(&d), resources(&r) {
}

bool te::classic_ui::input() {
    double mouse_x, mouse_y;
    glfwGetCursorPos(input_win->hnd.get(), &mouse_x, &mouse_y);
    cursor_pos.x = mouse_x;
    cursor_pos.y = mouse_y;

    prev_mouse_down = mouse_down;
    mouse_down = glfwGetMouseButton(input_win->hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouse_falling = prev_mouse_down && !mouse_down;
    mouse_rising = !prev_mouse_down && mouse_down;

    auto to_remove = windows.end();
    for (auto it = windows.begin(); it != windows.end(); it++) {
        do_generator_window(*it);
        /*
        if (do_generator_window(*it)) {
            to_remove = it;
        }*/
    }
    if (to_remove != windows.end()) {
        windows.erase(to_remove);
    }

    return false;
}

bool te::classic_ui::do_button(button& b) {
    bool clicked = false;
    auto& tex = resources->lazy_load<te::gl::texture2d>(b.fname);
    const glm::vec2 tex_size {tex.width / 2.0, tex.height};

    bool mouse_inside = cursor_pos.x >= b.tl.x && cursor_pos.x <= (b.tl.x + tex_size.x)
                     && cursor_pos.y >= b.tl.y && cursor_pos.y <= (b.tl.y + tex_size.y);
    if (mouse_down && mouse_inside) {
        b.depressed = true;
    } else if (b.depressed && !mouse_down) {
        b.depressed = false;
        clicked = mouse_inside;
    }

    const glm::vec2 tex_pos = b.depressed ? glm::vec2{0.5, 1.0} : glm::vec2{1.0, 1.0};
    draw->image(tex, b.tl, tex_size, tex_pos, {0.5, 1.0});

    return clicked;
}

void te::classic_ui::open_generator(entt::entity inspected) {
    auto it = std::find_if (
        windows.begin(),
        windows.end(),
        [inspected](generator_window& w) {
            return w.inspected == inspected;
        }
    );
    if (it == windows.end()) {
        windows.push_back (
            generator_window {
                .drag = drag_window {
                    .fname = "080",
                    .pos = {100, 100},
                    .title = model->entities.get<te::named>(inspected).name,
                    .close = button {"assets/a_ui,6.{}/041.png", glm::vec2{100, 100} + glm::vec2{226, 12}},
                    .drag_br = {256, 44}
                },
                .inspected = inspected
            }
        );
    } else {
        std::rotate(it, std::next(it), windows.end());
    }
}

void te::classic_ui::do_drag_window(te::classic_ui::drag_window& w) {
    auto& frame_tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}.png", w.fname));
    draw->image(frame_tex, w.pos);
    draw->text(w.title, w.pos + glm::vec2{16, 28}, {"Alegreya_Sans_SC/AlegreyaSansSC-Regular.ttf", 8.0});
    if (do_button(w.close)) {
        spdlog::debug("Should close {}", w.title);
    }
}

void te::classic_ui::do_generator_window(te::classic_ui::generator_window& win) {
    do_drag_window(win.drag);
    auto generator = model->entities.try_get<te::generator>(win.inspected);
    // Output icon
    if (auto tex = model->entities.try_get<te::render_tex>(generator->output)) {
        draw->image(resources->lazy_load<te::gl::texture2d>(tex->filename), win.drag.pos + glm::vec2{38, 85});
    }
    // Status
    draw->text (
        "Producing",
        win.drag.pos + glm::vec2{39, 73},
        {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7},
        {165/255.0, 219/255.0, 255/255.0, 255/255.0}
    );
    // Output name
    draw->text (
        model->entities.get<te::named>(generator->output).name,
        win.drag.pos + glm::vec2{70, 101},
        {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 7},
        {255/255.0, 195/255.0, 66/255.0, 255/255.0}
    );
    // Output progress
    draw->image(resources->lazy_load<te::gl::texture2d>("assets/a_ui,6.{}/116.png"), win.drag.pos + glm::vec2{33, 122});
    draw->rect(win.drag.pos + glm::vec2{34, 123}, {189.0 * generator->progress, 8}, {57/255.0, 158/255.0, 222/255.0, 255/255.0});
}

void te::classic_ui::render() {
    draw->render();
}
