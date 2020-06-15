#include <te/ui_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>

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
    te::gl::texture<GL_TEXTURE_2D> tex2d {gl.make_hnd<te::gl::texture_hnd>(glGenTextures)};
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
ft::face& te::ui_renderer::face(te::face_ref key) {
    auto it = faces.find(key);
    if (it == faces.end()) {
        auto emplaced = faces.emplace (
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(ft.make_face(fmt::format("assets/fonts/{}", key.first).c_str(), key.second))
        );
        return emplaced.first->second;
    } else {
        return it->second;
    }
}

void te::ui_renderer::image(te::gl::texture2d& tex, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 src_pos, glm::vec2 src_size) {
    using namespace glm;
    quads.emplace_back (
        quad {
            vertex {dest_pos,                                src_pos + vec2{0.0f, 0.0f} * src_size, vec4{1.0f}},
            vertex {dest_pos + vec2{0.0f, 1.0f} * dest_size, src_pos + vec2{0.0f, 1.0f} * src_size, vec4{1.0f}},
            vertex {dest_pos + vec2{1.0f, 0.0f} * dest_size, src_pos + vec2{1.0f, 0.0f} * src_size, vec4{1.0f}},
            vertex {dest_pos + vec2{1.0f, 1.0f} * dest_size, src_pos + vec2{1.0f, 1.0f} * src_size, vec4{1.0f}}
        }
    );
    textures.emplace_back(&tex);
}

void te::ui_renderer::image(te::gl::texture2d& tex, glm::vec2 dest_pos, glm::vec2 dest_size) {
    image(tex, dest_pos, dest_size, glm::vec2{0.0f, 0.0f}, glm::vec2{1.0f, 1.0f});
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

te::gl::texture2d& te::ui_renderer::glyph_texture(te::face_ref face_key, ft::glyph_index glyph_key) {
    auto it = glyph_textures.find(glyph_key);
    if (it == glyph_textures.end()) {
        FT_GlyphSlotRec glyph = face(face_key)[glyph_key];
        auto emplaced = glyph_textures.emplace (
            std::piecewise_construct,
            std::forward_as_tuple(glyph_key),
            std::forward_as_tuple(gl->make_texture(glyph))
        );
        return emplaced.first->second;
    } else {
        return it->second;
    }
}

void te::ui_renderer::text(std::string_view str, glm::vec2 cursor, face_ref fref) {
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
            {
                glyph.bitmap.width,
                glyph.bitmap.rows
            }
        );
        cursor.x += pos[i].x_advance / 64.0;
        cursor.y += pos[i].y_advance / 64.0;
    }
}

/*
void te::ui_renderer::centered_text(std::string_view str, glm::vec2 cursor, double max_width, double pts) {
    auto shaping_buffer = hb::buffer::shape(face(pts), str);
    unsigned int len = hb_buffer_get_length (shaping_buffer.hnd.get());
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos (shaping_buffer.hnd.get(), nullptr);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions (shaping_buffer.hnd.get(), nullptr);

    double width = 0.0;
    for (unsigned int i = 0; i < len; i++) {
        width += pos[i].x_advance / 64.0 + 4.0;
    }

    double spare = max_width - width;
    cursor.x += spare / 2.0;

    for (unsigned int i = 0; i < len; i++) {
        ft::glyph_index gix { info[i].codepoint };
        double x_offset = pos[i].x_offset / 64.0;
        double y_offset = pos[i].y_offset / 64.0;
        auto glyph = face(pts)[gix];
        texquad(glyph_texture(pts, gix), {cursor.x + x_offset + glyph.bitmap_left, cursor.y + y_offset - glyph.bitmap_top}, {glyph.bitmap.width, glyph.bitmap.rows});
        cursor.x += pos[i].x_advance / 64.0 + 4.0; // added x advance to make it wide
        cursor.y += pos[i].y_advance / 64.0;
    }
}

bool te::ui_renderer::button(std::string_view label, glm::vec2 tl, double pts) {
    glm::vec2 baseline = tl + glm::vec2{0.0f, pts};
    text(label, baseline, pts);
    glm::vec2 br = baseline + glm::vec2{100.0f, 0.0f};
    double mouse_x; double mouse_y;
    glfwGetCursorPos(win->hnd.get(), &mouse_x, &mouse_y);
    if (glfwGetMouseButton(win->hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE
        && mouse_x >= tl.x && mouse_x <= br.x && mouse_y >= tl.y && mouse_y <= br.y) {
        return true;
    } else {
        return false;
    }
}

bool te::ui_renderer::button(te::gl::texture2d&, glm::vec2 pos, glm::vec2 size) {
    return false;
}*/

#include <spdlog/spdlog.h>
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

void te::ui_renderer::input() {
    last_mouse_down = mouse_down;
    mouse_down = glfwGetMouseButton(win->hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}
