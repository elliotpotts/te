#include <te/colour_picker.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <array>

te::colour_picker::colour_picker(te::window& win):
    win(win),
    program(win.gl.link(win.gl.compile(te::file_contents("shaders/colour_pick_vertex.glsl"), GL_VERTEX_SHADER),
                        win.gl.compile(te::file_contents("shaders/colour_pick_fragment.glsl"), GL_FRAGMENT_SHADER),
                        te::gl::common_attribute_names)),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    colour_fbuffer(win.gl.make_framebuffer()),
    attachment(win.gl.make_renderbuffer(win.width(), win.height()))
{
    colour_fbuffer.bind();
    attachment.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *attachment.hnd);
}

te::colour_picker::instanced& te::colour_picker::instance(te::primitive& primitive) {
    auto instance_it = instances.find(&primitive);
    if (instance_it != instances.end()) {
        return instance_it->second;
    }
    auto instance_attribute_buffer = win.gl.make_buffer<GL_ARRAY_BUFFER>();
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
            win.gl.make_vertex_array(inputs)
        }
    );
    glVertexAttribDivisor(te::gl::INSTANCE_OFFSET, 1);
    glVertexAttribDivisor(te::gl::INSTANCE_COLOUR, 1);
    glBindVertexArray(0);
    return it->second;
}

void te::colour_picker::draw(instanced& instanced, const glm::mat4& model_mat, const te::camera& cam, int count) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model_mat));
    instanced.vertex_array.bind();
    //TODO: figure out why element buffer isn't part of VAO state
    instanced.primitive.inputs.elements.bind();
    glDrawElementsInstanced (
        instanced.primitive.mode,
        instanced.primitive.element_count,
        instanced.primitive.element_type,
        reinterpret_cast<void*>(instanced.primitive.element_offset),
        count
   );
}
