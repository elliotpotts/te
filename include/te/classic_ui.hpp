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
#include <optional>

namespace te::ui {
    struct button {
        boost::signals2::signal<void()> clicked;
    };

    enum class text_align { left, justify, right };
    struct offset_run {
        glm::vec2 offset;
        text_run run;
    };
    struct paragraph {
        std::vector<offset_run> runs;
    };

    struct roster_panel {
    };
    struct routes_panel {
    };
    struct construction_panel {
    };
    struct technologies_panel {
    };
    struct panels {
    };
    struct bottom_bar {
        std::optional<paragraph> info;
    };

    struct window {
        te::gl::texture2d* frame;
        button close;
        window(int frame_img);
    };
    struct market_window : public window {
    };
    struct production_window : public window {
    };
    struct dwelling_window : public window {
    };
    struct generator_window : public window {
    };

    struct ingame_root {
        panels left_panels;
        bottom_bar bottom;
    };

    struct root {
        te::window& input_win;
        canvas_renderer& canvas;
        te::cache<asset_loader>& assets;
        void ui_img(int number, glm::vec2 dest);

        paragraph make_paragraph(text_align align, double width_avail, double line_height, font fnt, std::string_view text);

        void render(paragraph&, glm::vec2 o);
        void render(bottom_bar&);
        void render(ingame_root&);

    public:
        root(te::window& win, canvas_renderer& canvas, cache<asset_loader>& loader);

        ingame_root ingame;
        boost::signals2::signal<void()> on_click;

        bool capture_input();
        void render(sim& model);
    };
}
#endif
