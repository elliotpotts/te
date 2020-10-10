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
#include <list>
#include <boost/signals2.hpp>

namespace te {
    constexpr glm::vec4 col_white = {1.0f, 1.0f, 1.0f, 1.0f};
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
        std::vector<te::gl::texture2d*> textures; // back to front order

        gl::buffer<GL_ARRAY_BUFFER> quad_attributes;
        input_description quad_input;
        static input_description make_quad_input(gl::buffer<GL_ARRAY_BUFFER>&);
        gl::vao quad_vao;

        ft::ft ft;
        std::map<fontspec, ft::face, fontspec_comparator> faces;
        ft::face& face(fontspec);
        std::map<fontspec, std::unordered_map<ft::glyph_index, te::gl::texture2d>, fontspec_comparator> glyph_textures;
        te::gl::texture2d& glyph_texture(fontspec, ft::glyph_index);

    public:
        ui_renderer(window&);
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 tex_pos, glm::vec2 tex_size, glm::vec4 colour = glm::vec4{1.0});
        void image(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec4 colour = glm::vec4{1.0});
        bool image_button(te::gl::texture2d&, glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec2 tex_pos_up, glm::vec2 tex_pos_down, glm::vec2 tex_size);
        void rect(glm::vec2 dest_pos, glm::vec2 dest_size, glm::vec4 colour);
        void text(std::string_view str, glm::vec2 cursor, fontspec font);

        void render();
    };

    struct classic_ui {
        struct rect {
            std::string name;
            std::string fname;
            glm::vec2 offset;
            glm::vec2 size;
            bool mouse_was_inside = false;
            bool mouse_inside = false;
            bool click_started = false;
            boost::signals2::signal<void()> on_mouse_enter;
            boost::signals2::signal<void()> on_mouse_leave;
            boost::signals2::signal<void()> on_mouse_down;
            boost::signals2::signal<void()> on_mouse_up;
            boost::signals2::signal<void()> on_click;
        };
        void input(glm::vec2 o, rect&);
        void draw_ui(glm::vec2 o, rect&);

        struct label {
            glm::vec2 offset;
            fontspec font;
            std::string text;
        };
        void input(glm::vec2 o, label&);
        void draw_ui(glm::vec2 o, label&);

        struct button : public rect {
        };
        void draw_ui(glm::vec2 o, button& b);

        struct drag_window {
            unsigned id;
            glm::vec2 offset;
            rect frame;
            button close;
            label title;
        };
        void input(glm::vec2 o, drag_window&);
        void draw_ui(glm::vec2 o, drag_window&);

        struct generator_window : public drag_window {
            entt::entity inspected;
            label status;
            label output_name;
            rect output_icon;
        };
        void input(glm::vec2 o, generator_window&);
        void draw_ui(glm::vec2 o, generator_window&);

        bool mouse_inside(glm::vec2 tl, glm::vec2 br) const;

        te::sim* model;
        window* input_win;
        ui_renderer* draw;
        te::cache<asset_loader>* resources;

        glm::dvec2 prev_cursor_pos;
        glm::dvec2 cursor_pos;
        bool mouse_was_down;
        bool mouse_is_down;

        bool enter_absorbed;
        bool leave_absorbed;
        bool down_absorbed;
        bool up_absorbed;
        bool click_absorbed;

        std::list<generator_window> windows; // stored back to front
        rect behind;

        decltype(windows)::iterator to_close;
        decltype(windows)::iterator to_bring_to_front;
        glm::vec2 drag_offset;
        std::optional<unsigned> dragging;
    public:
        classic_ui(te::sim&, window&, ui_renderer&, te::cache<asset_loader>&);

        void open_generator(entt::entity);

        bool input();
        void render();
    };
}
#endif
