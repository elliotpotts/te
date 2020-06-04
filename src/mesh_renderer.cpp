#include <te/mesh_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

te::mesh_renderer::mesh_renderer(gl::context& ogl):
    gl(ogl),
    program(gl.link(gl.compile(te::file_contents("shaders/instance_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/instance_fragment.glsl"), GL_FRAGMENT_SHADER),
                    te::gl::common_attribute_names)),
    view(program.uniform("view")),
    proj(program.uniform("projection")),
    model(program.uniform("model")),
    sampler(program.uniform("tex"))
{
}

te::mesh_renderer::instanced& te::mesh_renderer::instance(te::primitive& primitive) {
    auto instance_it = instances.find(&primitive);
    if (instance_it != instances.end()) {
        return instance_it->second;
    }
    auto instance_attribute_buffer = gl.make_buffer<GL_ARRAY_BUFFER>();
    auto inputs { primitive.inputs };
    inputs.attributes.emplace_back ( te::attribute_source {
        instance_attribute_buffer,
        te::gl::INSTANCE_OFFSET,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec2) + sizeof(glm::vec3),
        reinterpret_cast<void*>(0)
    });
    inputs.attributes.emplace_back ( te::attribute_source {
        instance_attribute_buffer,
        te::gl::INSTANCE_COLOUR,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec2) + sizeof(glm::vec3),
        reinterpret_cast<void*>(sizeof(glm::vec2))
    });
    auto [it, emplaced] = instances.emplace (
        &primitive,
        instanced {
            primitive,
            std::move(instance_attribute_buffer),
            gl.make_vertex_array(inputs)
        }
    );
    glVertexAttribDivisor(te::gl::INSTANCE_OFFSET, 1);
    glVertexAttribDivisor(te::gl::INSTANCE_COLOUR, 1);
    glBindVertexArray(0);
    return it->second;
}

void te::mesh_renderer::draw(instanced& instanced, const glm::mat4& model_mat, const te::camera& cam, int count) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(view, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(model_mat));
    //TODO: do we need a default sampler?
    GLuint unit = 0;
    for (auto unit_binding : instanced.primitive.texture_unit_bindings) {
        if (unit_binding.sampler) {
            unit_binding.sampler->bind(unit);
        }
        if (unit_binding.texture) {
            unit_binding.texture->activate(unit);
        }
        unit++;
    }
    instanced.vertex_array.bind();
    //TODO: figure out why element buffer isn't part of VAO state
    instanced.primitive.inputs.elements->bind();
    glDrawElementsInstanced (
        instanced.primitive.mode,
        instanced.primitive.element_count,
        instanced.primitive.element_type,
        reinterpret_cast<void*>(instanced.primitive.element_offset),
        count
    );
}
