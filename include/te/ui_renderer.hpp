#ifndef TE_UI_RENDERER_HPP_INCLUDED
#define TE_UI_RENDERER_HPP_INCLUDED

#include <te/gl.hpp>
#include <te/mesh.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <ft/ft.hpp>
#include <ft/face.hpp>
#include <hb/buffer.hpp>
#include <string_view>
#include <te/window.hpp>
#include <string>
#include <utility>
#include <map>

namespace te {
    using face_ref = std::pair<std::string, double>;

    struct fontspec {
        std::string filename;
        double pts;
        double aspect;
        glm::vec4 colour;
    };

    class ui_renderer {
        te::window* win;
        gl::context* gl;
        gl::program program;
        GLint projection;
        GLint sampler_uform;
        gl::sampler sampler;
        static te::gl::texture2d make_white1(te::gl::context&);
        gl::texture2d white1;

        struct vertex {
            glm::vec2 pos;
            glm::vec2 uv;
            glm::vec4 colour;
        };
        using quad = std::array<vertex, 4>;
        std::vector<quad> quads;
        std::vector<te::gl::texture2d*> textures;

        gl::buffer<GL_ARRAY_BUFFER> quad_attributes;
        input_description quad_input;
        static input_description make_quad_input(gl::buffer<GL_ARRAY_BUFFER>&);
        gl::vao quad_vao;

        ft::ft ft;
        std::map<face_ref, ft::face> faces;
        ft::face& face(face_ref fref);
        std::unordered_map<ft::glyph_index, te::gl::texture2d> glyph_textures;
        te::gl::texture2d& glyph_texture(face_ref, ft::glyph_index);

        glm::vec2 mouse;
        bool mouse_down = false;
        bool last_mouse_down = false;
    public:
        ui_renderer(window&);
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos, glm::vec2 tex_size);
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size);
        bool image_button(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos_up, glm::vec2 tex_pos_down, glm::vec2 tex_size);
        void rect(glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec4 colour);
        void text(std::string_view str, glm::vec2 cursor, face_ref fref);

        /*
        void centered_text(std::string_view str, glm::vec2 pos, double max_width, double pts);
        bool button(std::string_view label, glm::vec2 pos, double pts);
        bool button(te::gl::texture2d&, glm::vec2 pos, glm::vec2 size);
        */

        void input();
        void render();
    };
}
#endif
