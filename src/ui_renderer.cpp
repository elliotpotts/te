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
    quad_vao { gl->make_vertex_array(quad_input) }
{
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
