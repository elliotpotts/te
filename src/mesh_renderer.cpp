#include <te/mesh_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>

te::mesh_renderer::mesh_renderer(gl::context& ogl):
    gl(ogl),
    program(gl.link(gl.compile(te::file_contents("shaders/basic_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/basic_fragment.glsl"), GL_FRAGMENT_SHADER),
                    te::gl::common_attribute_locations)),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    tint_uniform(program.uniform("tint")),
    sampler_uniform(program.uniform("tex"))
{
}

void te::mesh_renderer::draw(te::mesh& the_mesh, const glm::mat4& model, const te::camera& cam, glm::vec4 tint) {
    glUseProgram(*program.hnd);
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glUniform4fv(tint_uniform, 1, glm::value_ptr(tint));
    for (auto& prim : the_mesh.primitives) {
        //TODO: do we need a default sampler?
        if (prim.sampler) prim.sampler->bind(0);
        if (prim.texture) prim.texture->activate(0);
        glBindVertexArray(prim.vao);
        glDrawElements(prim.mode, prim.count, prim.type, reinterpret_cast<void*>(prim.offset));
    }
}
