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
#include <te/sim.hpp>
#include <te/cache.hpp>

namespace te {
    struct fontspec {
        std::string filename;
        double pts;
        double aspect;
        glm::vec4 colour;
    };
    struct fontspec_comparator {
        bool operator()(const fontspec&, const fontspec&) const;
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
        std::map<fontspec, ft::face, fontspec_comparator> faces;
        ft::face& face(fontspec);
        std::map<fontspec, std::unordered_map<ft::glyph_index, te::gl::texture2d>, fontspec_comparator> glyph_textures;
        te::gl::texture2d& glyph_texture(fontspec, ft::glyph_index);

        glm::vec2 mouse;
        bool mouse_down = false;
        bool last_mouse_down = false;
    public:
        ui_renderer(window&);
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec4 colour = glm::vec4{1.0});
        bool image_button(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos_up, glm::vec2 tex_pos_down, glm::vec2 tex_size);
        void rect(glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec4 colour);
        void text(std::string_view str, glm::vec2 cursor, fontspec font, glm::vec4 colour = glm::vec4{1.0});

        void render();
    };

    struct classic_ui {
        struct button_s {
            glm::vec2 tl;
            glm::vec2 br;
            bool depressed;
        };
        bool button(glm::vec2 tl, glm::vec2 size);

        struct drag_window {
            std::string_view fname;
            glm::vec2 pos;
            std::string_view title;
            bool dragging = false;
        };
        bool drag_window(drag_window w);

        struct window_s {
            glm::vec2 pos;
            bool dragging;
            entt::entity inspected;
        };
        bool generator_window(window_s&);

        te::sim* model;
        window* input_win;
        ui_renderer* draw;
        te::cache<asset_loader>* resources;

        glm::vec2 mouse;
        bool mouse_down;
        bool last_mouse_down;
        std::vector<window_s> windows;
    public:
        classic_ui(te::sim&, window&, ui_renderer&, te::cache<asset_loader>&);
        bool input();
        void render();
    };
}
#endif
