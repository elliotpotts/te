#include <te/colour_picker.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <array>

te::colour_picker::colour_picker(te::window& win):
    win(win),
    program(win.gl.link(win.gl.compile(te::file_contents("shaders/colour_pick_vertex.glsl"), GL_VERTEX_SHADER),
                        win.gl.compile(te::file_contents("shaders/colour_pick_fragment.glsl"), GL_FRAGMENT_SHADER),
                        te::gl::common_attribute_locations)),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    pick_colour_uniform(program.uniform("pick_colour")),
    colour_fbuffer(win.gl.make_framebuffer()),
    attachment(win.gl.make_renderbuffer(win.width, win.height))
{
    colour_fbuffer.bind();
    attachment.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *attachment.hnd);
}

void te::colour_picker::draw(te::mesh& the_mesh, const glm::mat4& model, std::uint32_t id, const te::camera& cam) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glm::vec3 rgb {
        (id & 0x000000ff),
        (id & 0x0000ff00) >> 8,
        (id & 0x00ff0000) >> 16
    };
    glUniform3fv(pick_colour_uniform, 1, glm::value_ptr(rgb / 255.0f));
    for (auto& prim : the_mesh.primitives) {
        //TODO: do we need a default sampler?
        if (prim.sampler) prim.sampler->bind(0);
        if (prim.texture) prim.texture->activate(0);
        glBindVertexArray(prim.vao);
        glDrawElements(prim.mode, prim.count, prim.type, reinterpret_cast<void*>(prim.offset));
    }
}
