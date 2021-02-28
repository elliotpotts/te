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
        std::string id;
        bool visible;
        glm::vec2 size;
        glm::vec2 offset;
        glm::vec4 colour;
        std::string image;
        glm::vec2 image_size;
        glm::vec2 image_offset;
        std::string text;
        font text_font;
        boost::signals2::signal<void(double, double)> on_mouse_enter;
        boost::signals2::signal<void(double, double)> on_mouse_move;
        boost::signals2::signal<void(double, double)> on_mouse_leave;
        boost::signals2::signal<void(double, double)> on_mouse_down;
        boost::signals2::signal<void(double, double)> on_mouse_up;
        boost::signals2::signal<void(double, double)> on_click;
        //boost::signals2::signal<void(const int, const int, const int, const int)> on_key;
        //boost::signals2::signal<void(std::string)> on_text;
        std::vector<node*> children;
        node();
    };

    struct button {
        bool pressed;
        node* n;
        button(const char* i, bool value);
        void update();
    };

    struct generator_window {
        node* root;
        node* title;
        //button close;
        node* status_text;
        node* commodity_icon;
        node* commodity_label;
        generator_window();
    };

    struct uui {
        node* top_bar;

        node* construction_panel;
        node* cons_0_back;
        node* cons_0_text;
        node* cons_1_back;
        node* cons_1_text;

        node* roster_panel;
        node* orders_panel;
        node* routes_panel;
        node* tech_panel;

        node* bottom_bar;
        node* bottom_bar_text;

        button* roster_button;
        button* routes_button;
        button* construction_button;
        button* tech_button;

        node* panel_open;
        button* panel_button;

        uui(node& root);
        void toggle_button(button* btn, node* pnl);
    };


    struct root {
        window* input_win;
        canvas_renderer* canvas;
        te::cache<asset_loader>* resources;
        node dom;
        uui* foo;

        node* focused;
        node* clicking;
        node* over;
        node* dragging;
        glm::vec2 drag_start;
        void cursor_move(double x, double y);

        void render(glm::vec2 tl, node& n);
    public:
        root(te::sim&, window&, canvas_renderer&, te::cache<asset_loader>&);
        bool input();
        void render();
    };
}
#endif
