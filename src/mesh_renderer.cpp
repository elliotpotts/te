#include <te/mesh_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
te::mesh_renderer::mesh_renderer(std::string filename):
    program(te::gl::link(te::gl::compile(te::file_contents("shaders/basic_vertex.glsl"), GL_VERTEX_SHADER),
                         te::gl::compile(te::file_contents("shaders/basic_fragment.glsl"), GL_FRAGMENT_SHADER))),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    sampler_uniform(program.uniform("tex")),
    the_mesh(load_mesh(filename, program))
{
}
void te::mesh_renderer::draw(const te::camera& cam) {
    glUseProgram(*program.hnd);
    glm::mat4 model =
        glm::scale (
            glm::translate (
                glm::rotate (
                    glm::rotate (
                        glm::mat4 { 1.0f },
                        glm::half_pi<float>(),
                        glm::vec3 { 1.0f, 0.0f, 0.0f }
                    ),
                    glm::half_pi<float>(),
                    glm::vec3 { 0.0f, 1.0f, 0.0f }
                ),
                glm::vec3{ 0.0f, 0.0f, 10.0f }
            ),
            glm::vec3 {20.0f, 20.0f, 20.0f}
        );
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    for (auto& prim : the_mesh.primitives) {
        glBindTexture(GL_TEXTURE_2D, prim.texture);
        glBindVertexArray(prim.vao);
        glDrawElements(prim.mode, prim.count, prim.type, reinterpret_cast<void*>(prim.offset));
    }
}
