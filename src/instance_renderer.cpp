#include <te/instance_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

te::instance_renderer::instance_renderer(gl::context& ogl):
    gl(ogl),
    program(gl.link(gl.compile(te::file_contents("shaders/instance_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/instance_fragment.glsl"), GL_FRAGMENT_SHADER),
                    te::gl::common_attribute_locations)),
    view(program.uniform("view")),
    proj(program.uniform("projection")),
    model(program.uniform("model")),
    sampler(program.uniform("tex"))
{
}

te::instanced_primitive& te::instance_renderer::instance(te::primitive& instancee) {
    auto instance_it = instances.find(&instancee);
    if (instance_it != instances.end()) {
        return instance_it->second;
    }
    auto instance_attribute_buffer = gl.make_buffer<GL_ARRAY_BUFFER>();
    auto inputs { instancee.inputs };
    inputs.attributes.emplace_back ( te::attribute_source {
        instance_attribute_buffer,
        4,
        2,
        GL_FLOAT,
        GL_FALSE,
        0,
        reinterpret_cast<void*>(0)
    });
    auto [it, emplaced] = instances.emplace (
        &instancee,
        instanced_primitive {
            instancee,
            std::move(instance_attribute_buffer),
            gl.make_vertex_array(inputs)
        }
    );
    glVertexAttribDivisor(4, 1);
    glBindVertexArray(0);
    return it->second;
}

void te::instance_renderer::draw(te::instanced_primitive& instanced, const glm::mat4& model_mat, const te::camera& cam, int count) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(view, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(model_mat));
    //TODO: do we need a default sampler?
    GLuint unit = 0;
    for (auto unit_binding : instanced.instancee.texture_unit_bindings) {
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
    instanced.instancee.inputs.elements.bind();
    glDrawElementsInstanced (
        instanced.instancee.mode,
        instanced.instancee.element_count,
        instanced.instancee.element_type,
        reinterpret_cast<void*>(instanced.instancee.element_offset),
        count
   );
}
