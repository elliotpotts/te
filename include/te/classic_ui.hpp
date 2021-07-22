#ifndef TE_CLASSIC_UI_HPP_INCLUDED
#define TE_CLASSIC_UI_HPP_INCLUDED

#include <memory>
#include <optional>
#include <stack>
#include <string_view>
#include <variant>
#include <boost/signals2.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <te/cache.hpp>
#include <te/canvas_renderer.hpp>
#include <te/window.hpp>

namespace te::ui {
    enum class text_align { left, justify, right };
    struct offset_run {
        glm::vec2 offset;
        text_run run;
    };
    struct paragraph {
        std::vector<offset_run> runs;
    };

    enum class position_t { static_, relative, absolute };

    struct node {
        std::string_view name;
        node* parent;
        // style
        position_t position = position_t::absolute;
        glm::vec2 offset = {0.0, 0.0};
        glm::vec2 size = {50.0, 50.0};
        glm::vec4 bg_colour;
        std::string bg_image = "";
        glm::vec2 bg_tl = {0.0, 0.0};
        glm::vec2 bg_br = {1.0, 1.0};
        void text(std::string text);
        font font_;
        // events
        boost::signals2::signal<bool(node&)> on_mouse_enter;
        boost::signals2::signal<bool(node&, glm::vec2, glm::vec2)> on_mouse_move;
        boost::signals2::signal<bool(node&)> on_mouse_leave;
        boost::signals2::signal<bool(node&)> on_mouse_down;
        boost::signals2::signal<bool(node&)> on_mouse_up;
        boost::signals2::signal<bool(node&, int, int, int)> on_click;
        boost::signals2::signal<bool(node&, int, int, int, int)> on_key;
        std::vector<std::shared_ptr<node>> children;
        virtual ~node() = default;
        //protected:
        std::string text_str;
        template<typename T, typename... Args>
        auto fire_event(T node::* ev, Args&&... args) {
            node* target = this;
            while (target != nullptr) {
                auto& signal = (*target).*ev;
                boost::optional<bool> bubble = signal(std::forward<Args>(args)...);
                target = bubble && !*bubble
                    ? nullptr
                    : target->parent;
            }
        }
    };

    struct root;

    struct button : node {
        bool pressed;
        boost::signals2::signal<void(button&)> on_press;
        node* node;
        button(ui::root&, int image);
    };

    enum traversal_order { Inorder, Preorder, Postorder, BreadthFirst, DepthFirst };

    struct root : public node {
        te::window& input_win;
        canvas_renderer& canvas;
        te::cache<asset_loader>& assets;
        void ui_img(int number, glm::vec2 dest);
        void ui_btn(int number, bool down, glm::vec2 dest);
        paragraph make_paragraph(text_align align, double width_avail, double line_height, font fnt, std::string_view text);

        glm::vec2 mouse_pos;
        node* under_cursor;

        template<typename F>
        void traverse_dfs(F t, node& n) {
            //TODO: outside layout
            const glm::vec2 initial_cursor = cursor;
            const glm::vec2 initial_lpa = lpa;
            glm::vec2 final_;
            switch (n.position) {
            case position_t::relative:
                final_ = cursor + n.offset;
                break;
            case position_t::absolute:
                final_ = lpa + n.offset;
                break;
            case position_t::static_:
                final_ = n.offset;
                break;
            default:
                break;
            }
            t(n, final_, n.size);
            cursor = final_;
            if (n.position != position_t::static_) {
                lpa = final_;
            }
            for (auto& child : n.children) {
                traverse_dfs(t, *child);
                switch (n.position) {
                case position_t::relative:
                    cursor.y += child->size.y;
                    break;
                case position_t::absolute:
                case position_t::static_:
                    break;
                }
            }
            cursor = initial_cursor;
            lpa = initial_lpa;
        }

        // layout context
        glm::vec2 cursor;
        glm::vec2 lpa;
    public:
        root(te::window& win, canvas_renderer& canvas, cache<asset_loader>& loader);
        bool capture_input();
        void render(sim& model);

        boost::signals2::signal<bool(node&)> global_on_mouse_enter;
        boost::signals2::signal<bool(node&, glm::vec2, glm::vec2)> global_on_mouse_move;
        boost::signals2::signal<bool(node&)> global_on_mouse_leave;
        boost::signals2::signal<bool(node&)> global_on_mouse_down;
        boost::signals2::signal<bool(node&)> global_on_mouse_up;
        boost::signals2::signal<bool(node&, int, int, int)> global_on_click;
        boost::signals2::signal<bool(node&, int, int, int, int)> global_on_key;
    };

    struct generator_window : node {
        std::shared_ptr<node> status;
        std::shared_ptr<node> progress_bar;
        generator_window(sim&, root&, entt::entity);
        void update(sim& model, root& ui, entt::entity e);
    };
}
#endif
