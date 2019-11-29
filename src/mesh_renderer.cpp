#include <te/mesh_renderer.hpp>
#include <te/util.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <complex>
te::mesh_renderer::mesh_renderer(gl::context& ogl, std::string filename):
    gl(ogl),
    program(gl.link(gl.compile(te::file_contents("shaders/basic_vertex.glsl"), GL_VERTEX_SHADER),
                    gl.compile(te::file_contents("shaders/basic_fragment.glsl"), GL_FRAGMENT_SHADER))),
    model_uniform(program.uniform("model")),
    view_uniform(program.uniform("view")),
    proj_uniform(program.uniform("projection")),
    sampler_uniform(program.uniform("tex")),
    the_mesh(load_mesh(gl, filename, program))
{
}
namespace {
    template<typename T = float, typename Clock = std::chrono::high_resolution_clock>
    struct phasor {
        const Clock::time_point birth;
        const T A;
        const T omega;
        const T phi;
        phasor(Clock::time_point birth, T A, T omega, T phi) : birth(birth), A(A), omega(omega), phi(phi) {
        }
        phasor(T A, T omega, T phi) : birth(Clock::now()), A(A), omega(omega), phi(phi) {
        }
        template<typename dt = std::chrono::milliseconds>
        std::complex<T> operator()(Clock::time_point t = Clock::now()) {
            return std::polar(A, omega * std::chrono::duration_cast<dt>(t - birth).count() * (static_cast<T>(dt::period::num) / static_cast<T>(dt::period::den)) + phi);
        }
    };
}
void te::mesh_renderer::draw(const te::camera& cam) {
    glUseProgram(*program.hnd);
    static phasor x {5.0f, glm::half_pi<float>(), 0.0f};
    glm::mat4 orient = glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});
    glm::mat4 turn = glm::rotate(glm::mat4{1.0f}, std::arg(x()), glm::vec3{0.0f, 0.0f, 1.0f});
    glm::mat4 upscale = glm::scale(glm::mat4{1.0f}, glm::vec3{10.0f, 10.0f, 10.0f});
    glm::mat4 move = glm::translate(glm::mat4{1.0f}, glm::vec3{x().real(), x().imag(), 1.0f});
    glm::mat4 model = move * upscale * turn * orient;
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(cam.projection()));
    for (auto& prim : the_mesh.primitives) {
        //TODO: do we need a default sampler?
        if (prim.sampler) prim.sampler->bind(0);
        if (prim.texture) prim.texture->activate(0);
        glBindVertexArray(prim.vao);
        glDrawElements(prim.mode, prim.count, prim.type, reinterpret_cast<void*>(prim.offset));
    }
}
