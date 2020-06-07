#include <te/ui_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>

te::input_description te::ui_renderer::make_quad_input(gl::buffer<GL_ARRAY_BUFFER>& buf) {
    te::input_description inputs;
    inputs.elements = nullptr;
    inputs.attributes.emplace_back ( te::attribute_source {
        buf, te::gl::POSITION, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec2), 0
    });
    inputs.attributes.emplace_back ( te::attribute_source {
        buf, te::gl::TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec2), reinterpret_cast<void*>(sizeof(glm::vec2))
    });
    return inputs;
}

te::ui_renderer::ui_renderer(te::gl::context& ctx):
    gl { &ctx },
    program { gl->link(gl->compile(te::file_contents("shaders/texquad_vertex.glsl"), GL_VERTEX_SHADER),
                       gl->compile(te::file_contents("shaders/texquad_fragment.glsl"), GL_FRAGMENT_SHADER),
                       te::gl::common_attribute_names) },
    projection { program.uniform("projection") },
    sampler_uform { program.uniform("tex") },
    sampler { gl->make_sampler() },
    quad_attributes { gl->make_buffer<GL_ARRAY_BUFFER>() },
    quad_input { make_quad_input(quad_attributes) },
    quad_vao { gl->make_vertex_array(quad_input) },
    face { ft.make_face("assets/fonts/Raleway-Black.ttf", 9) }
{
    sampler.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    sampler.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void te::ui_renderer::texquad(te::gl::texture2d& tex, glm::vec2 pos, glm::vec2 size) {
    struct vert {
        glm::vec2 pos;
        glm::vec2 tex;
    };
    static_assert(sizeof(vert) == sizeof(pos) + sizeof(size));
    std::array<vert, 4> quad = {{
        {{pos.x,          pos.y},          {0.0f, 0.0f}},
        {{pos.x         , pos.y + size.y}, {0.0f, 1.0f}},
        {{pos.x + size.x, pos.y},          {1.0f, 0.0f}},
        {{pos.x + size.x, pos.y + size.y}, {1.0f, 1.0f}}
    }};
    quad_attributes.bind();
    quad_attributes.upload(quad.begin(), quad.end(), GL_DYNAMIC_READ);

    glUseProgram(*program.hnd);
    glm::mat4 to_ndc = glm::ortho(0.0f, 1024.0f, 768.0f, 0.0f);
    glUniformMatrix4fv(projection, 1, GL_FALSE, glm::value_ptr(to_ndc));
    sampler.bind(0);
    tex.activate(0);
    quad_vao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

te::gl::texture2d& te::ui_renderer::glyph_texture(ft::glyph_index key) {
    auto it = glyph_textures.find(key);
    if (it == glyph_textures.end()) {
        FT_GlyphSlotRec glyph = face[key];
        auto emplaced = glyph_textures.emplace (
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(gl->make_texture(glyph))
        );
        return emplaced.first->second;
    } else {
        return it->second;
    }
}

void te::ui_renderer::text(std::string_view str, glm::vec2 cursor) {
    auto shaping_buffer = hb::buffer::shape(face, str);
    unsigned int len = hb_buffer_get_length (shaping_buffer.hnd.get());
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos (shaping_buffer.hnd.get(), nullptr);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions (shaping_buffer.hnd.get(), nullptr);

    for (unsigned int i = 0; i < len; i++) {
        ft::glyph_index gix { info[i].codepoint };
        double x_offset = pos[i].x_offset / 64.0;
        double y_offset = pos[i].y_offset / 64.0;
        auto glyph = face[gix];
        texquad(glyph_texture(gix), {cursor.x + x_offset + glyph.bitmap_left, cursor.y + y_offset - glyph.bitmap_top}, {glyph.bitmap.width, glyph.bitmap.rows});
        cursor.x += pos[i].x_advance / 64.0 + 4.0; // added x advance to make it wide
        cursor.y += pos[i].y_advance / 64.0;
    }
}


void te::ui_renderer::centered_text(std::string_view str, glm::vec2 cursor, double max_width) {
    auto shaping_buffer = hb::buffer::shape(face, str);
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
        auto glyph = face[gix];
        texquad(glyph_texture(gix), {cursor.x + x_offset + glyph.bitmap_left, cursor.y + y_offset - glyph.bitmap_top}, {glyph.bitmap.width, glyph.bitmap.rows});
        cursor.x += pos[i].x_advance / 64.0 + 4.0; // added x advance to make it wide
        cursor.y += pos[i].y_advance / 64.0;
    }
}
