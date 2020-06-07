#ifndef TE_UI_RENDERER_HPP_INCLUDED
#define TE_UI_RENDERER_HPP_INCLUDED

#include <te/gl.hpp>
#include <te/mesh.hpp>
#include <glm/vec2.hpp>
#include <ft/ft.hpp>
#include <ft/face.hpp>
#include <hb/buffer.hpp>
#include <string_view>

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

        ft::ft ft;
        ft::face face;
        std::unordered_map<ft::glyph_index, te::gl::texture2d> glyph_textures;
        te::gl::texture2d& glyph_texture(ft::glyph_index);
    public:
        ui_renderer(gl::context&);
        void texquad(te::gl::texture2d&, glm::vec2 pos, glm::vec2 size);
        void text(std::string_view str, glm::vec2 pos);
        void centered_text(std::string_view str, glm::vec2 pos, double max_width);
    };
}
#endif
