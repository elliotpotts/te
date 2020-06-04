#ifndef TE_UI_RENDERER_HPP_INCLUDED
#define TE_UI_RENDERER_HPP_INCLUDED

#include <te/gl.hpp>
#include <te/mesh.hpp>
#include <glm/vec2.hpp>

namespace te {
    class ui_renderer {
        gl::context* gl;
        gl::program program;
        GLint projection;
        GLint sampler_uform;
        gl::sampler sampler;

        gl::buffer<GL_ARRAY_BUFFER> quad_attributes;
        input_description quad_input;
        static input_description make_quad_input(gl::buffer<GL_ARRAY_BUFFER>&);
        gl::vao quad_vao;
    public:
        ui_renderer(gl::context&);
        void texquad(te::gl::texture2d&, glm::vec2 pos, glm::vec2 size);
    };
}
#endif
