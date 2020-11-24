#ifndef TE_CLASSIC_UI_HPP_INCLUDED
#define TE_CLASSIC_UI_HPP_INCLUDED

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
#include <memory>
#include <te/canvas_renderer.hpp>

namespace te {
    struct classic_ui {
        struct rect {
            std::string name;
            std::optional<std::string> fname;
            std::optional<glm::vec4> colour;
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

        using element = std::variant<rect, label>;

        struct control {
            std::vector<element> elements;
            std::vector<control> children;
        };

        struct button : public rect {
        };
        void draw_ui(glm::vec2 o, button& b);

        struct drag_window {
            unsigned id;
            glm::vec2 offset;
            rect frame;
            button close;
            virtual ~drag_window() = default;
            virtual void input(classic_ui&, glm::vec2) = 0;
            virtual void update(te::sim&) = 0;
            virtual void draw_ui(classic_ui&, glm::vec2) = 0;
        };

        struct console : public drag_window {
            std::vector<std::string> lines;
            console();
            virtual ~console() = default;
            virtual void input(classic_ui&, glm::vec2) override;
            virtual void update(te::sim&) override;
            virtual void draw_ui(classic_ui&, glm::vec2) override;
        };

        bool mouse_inside(glm::vec2 tl, glm::vec2 br) const;

        te::sim* model;
        window* input_win;
        canvas_renderer* canvas;
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

        unsigned next_id = 0;
        std::list<std::unique_ptr<drag_window>> windows; // stored back to front
        void show_window(std::unique_ptr<drag_window> w);
        rect behind;

        decltype(windows)::iterator to_close;
        decltype(windows)::iterator to_bring_to_front;
        glm::vec2 drag_offset;
        std::optional<unsigned> dragging;
    public:
        classic_ui(te::sim&, window&, canvas_renderer&, te::cache<asset_loader>&);
        bool input();
        void render();
    };
}
#endif
