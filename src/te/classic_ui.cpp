#include <te/canvas_renderer.hpp>
#include <te/classic_ui.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <te/components.hpp>
#include <scm.hpp>
#include <algorithm>

static te::font tefont {
    .filename = "Alegreya_Sans_SC/AlegreyaSansSC-Bold.tff",
    .pts = 8.0,
    .aspect = 1.0,
    .colour = {1.0, 1.0, 1.0, 1.0}
};

te::ui::paragraph te::ui::root::make_paragraph(text_align align, double width_avail, double line_height, font fnt, std::string_view text) {
    auto& face = canvas.face(fnt);
    double design_height;
    if (FT_IS_SCALABLE(face)) {
        design_height = face->height / 72.0;
    } else {
        design_height = 0;
    }
    design_height *= fnt.line_height;

    auto space_width = canvas.shape_run(" ", fnt).total_width;
    std::vector<text_run> words;
    auto word_begin = text.begin();
    auto word_end = std::find(text.begin(), text.end(), ' ');
    while (word_begin != text.end()) {
        words.push_back(canvas.shape_run({std::to_address(word_begin), std::to_address(word_end)}, fnt));
        std::string werd {word_begin, word_end};
        if (word_end == text.end()) {
            break;
        } else {
            word_end++;
            word_begin = word_end;
            word_end = std::find(word_begin, text.end(), ' ');
        }
    }
    glm::vec2 cursor {0, 0};
    std::vector<offset_run> runs;
    for (auto word : words) {
        if (cursor.x + word.total_width >= width_avail) {
            cursor.x = 0;
            cursor.y += design_height * line_height;
        }
        runs.push_back (
            offset_run {
                .offset = cursor,
                .run = word
            }
        );
        cursor.x += word.total_width + space_width;
        //spdlog::debug("({}, {})\n", cursor.x, cursor.y);
    }
    return paragraph { runs };
}

//------------------------------

void te::ui::root::ui_img(int number, glm::vec2 dest) {
    auto& tex = assets.lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{:3}.png", number));
    const glm::vec2 tex_size = glm::vec2{tex.width, tex.height};
    canvas.image(tex, dest, tex_size, {0.0, 0.0}, {1.0, 1.0});
}

te::ui::window::window(int frame_img) {
}

static bool inside_rect(glm::vec2 pt, glm::vec2 tl, glm::vec2 br) {
    return pt.x >= tl.x && pt.x <= br.x && pt.y >= tl.y && pt.y <= br.y;
}

te::ui::root::root(te::window& win, te::canvas_renderer& canvas, te::cache<asset_loader>& loader):
    input_win{win}, canvas{canvas}, assets{loader}
{
    win.on_mouse_button.connect([&](int button, int action, int mods) {
        if (action == GLFW_RELEASE) {
            on_click();
        }
    });
    /*
    w.on_cursor_move.connect([&](double x, double y) { cursor_move(x, y); });
    w.on_mouse_button.connect([&](int button, int action, int mods) {
        if (action == GLFW_PRESS && over) {
            over->on_mouse_down(last_mouse_x, last_mouse_y);
            clicking = over;
        } else if (action == GLFW_RELEASE && over) {
            over->on_mouse_up(last_mouse_x, last_mouse_y);
            if (over == clicking) {
                over->on_click(last_mouse_x, last_mouse_y);
                clicking = nullptr;
            }
        }
        if (action == GLFW_RELEASE && dragging) {
            dragging = nullptr;
        }
    });
    */
}

bool te::ui::root::capture_input() {
    return false;
}

void te::ui::root::render(te::sim& model) {
    render(ingame);
    canvas.render();
}

void te::ui::root::render(te::ui::paragraph& p, glm::vec2 o) {
    for (auto& run : p.runs) {
        for (auto& glyph : run.run.glyphs) {
            canvas.image(*glyph.texture, o + run.offset + glyph.offset);
        }
    }
}

void te::ui::root::render(te::ui::bottom_bar& bb) {
    ui_img(169, {0, 714});
    if (bb.info) {
        render(*bb.info, {273, 740});
    }
}

void te::ui::root::render(te::ui::ingame_root& ir) {
    render(ir.bottom);
}
