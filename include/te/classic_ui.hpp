#ifndef TE_CLASSIC_UI_HPP_INCLUDED
#define TE_CLASSIC_UI_HPP_INCLUDED

#include <boost/signals2.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <list>
#include <string_view>
#include <te/cache.hpp>
#include <te/canvas_renderer.hpp>
#include <te/window.hpp>

namespace te::ui {
    struct node {
        bool visible;
        glm::vec2 size;
        glm::vec2 offset;
        glm::vec4 colour;
        std::string image;
        glm::vec2 image_size;
        glm::vec2 image_offset;
        std::string text;
        font text_font;
        boost::signals2::signal<void()> on_mouse_enter;
        boost::signals2::signal<void()> on_mouse_leave;
        boost::signals2::signal<void()> on_mouse_down;
        boost::signals2::signal<void()> on_mouse_up;
        boost::signals2::signal<void()> on_click;
        std::list<node> children;
        node();
    };

    struct root {
        window* input_win;
        canvas_renderer* canvas;
        te::cache<asset_loader>* resources;
        node dom;

        node* over;
        void cursor_move(double x, double y);

        void render(glm::vec2 tl, node& n);
    public:
        root(te::sim&, window&, canvas_renderer&, te::cache<asset_loader>&);
        bool input();
        void render();
    };
}
#endif
