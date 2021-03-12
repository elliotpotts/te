#ifndef TE_CANVAS_RENDERER_HPP_INCLUDED
#define TE_CANVAS_RENDERER_HPP_INCLUDED
#include <te/gl.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <ft/ft.hpp>
#include <ft/face.hpp>
#include <string_view>
#include <te/window.hpp>
#include <string>
#include <utility>
#include <map>
#include <te/sim.hpp>
#include <te/cache.hpp>
#include <list>
#include <boost/signals2.hpp>
#include <memory>
#include <ibus/bus.hpp>

namespace te {
    struct font {
        std::string filename;
        double pts;
        double aspect;
        double line_height;
        glm::vec4 colour;
    };
    struct font_comparator {
        bool operator()(const font&, const font&) const;
    };

    struct text_run {
        struct glyph_instance {
            glm::vec2 offset;
            gl::texture2d* texture;
        };
        const std::vector<glyph_instance> glyphs;
        const double total_width;
        const font fnt;
    };

    struct canvas_renderer {
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
        std::vector<te::gl::texture2d*> textures; // back to front order

        gl::buffer<GL_ARRAY_BUFFER> quad_attributes;
        input_description quad_input;
        static input_description make_quad_input(gl::buffer<GL_ARRAY_BUFFER>&);
        gl::vao quad_vao;

        ft::ft ft;
        std::map<font, ft::face, font_comparator> faces;
        ft::face& face(font);
        std::map<font, std::unordered_map<ft::glyph_index, te::gl::texture2d>, font_comparator> glyph_textures;
        te::gl::texture2d& glyph_texture(font, ft::glyph_index);

    public:
        canvas_renderer(window&);
        // dest pos, dest size are in screen cords. tex pos, tex size are in uv cords.
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec4 colour = glm::vec4{1.0});
        bool image_button(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos_up, glm::vec2 tex_pos_down, glm::vec2 tex_size);
        void rect(glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec4 colour);

        // Origin is bottom-left corner for text
        text_run shape_run(std::string_view str, font fnt);
        void run(text_run run, glm::vec2 origin);
        void text(std::string_view str, glm::vec2 origin, font fnt);

        void render();
    };
}
#endif
