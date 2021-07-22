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

static bool inside_rect(glm::vec2 pt, glm::vec2 tl, glm::vec2 br) {
    return pt.x >= tl.x && pt.x <= br.x && pt.y >= tl.y && pt.y <= br.y;
}

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
    }
    return paragraph { runs };
}

te::ui::root::root(te::window& win, te::canvas_renderer& canvas, te::cache<asset_loader>& loader):
    input_win{win}, canvas{canvas}, assets{loader}, mouse_pos{0,0}
{
    parent = nullptr;
    name = "root";
    input_win.on_mouse_button.connect([&](int button, int action, int mods) {
        if (under_cursor) {
            under_cursor->fire_event(&node::on_click, *under_cursor, button, action, mods);
        }
        return true;
    });
    input_win.on_cursor_move.connect([&](double mouse_x, double mouse_y) {
        const glm::vec2 now_pos{mouse_x, mouse_y};
        node* now_under_cursor = nullptr;
        traverse_dfs([&](node& n, glm::vec2 tl, glm::vec2 size) {
            if (inside_rect(glm::vec2{mouse_x, mouse_y}, tl, tl + size)) {
                now_under_cursor = &n;
            }
        }, *this);
        if (now_under_cursor != under_cursor) {
            if (under_cursor) {
                under_cursor->fire_event(&node::on_mouse_leave, *under_cursor);
            }
            if (now_under_cursor) {
                now_under_cursor->fire_event(&node::on_mouse_enter, *now_under_cursor);
            }
        }
        under_cursor = now_under_cursor;
        if (under_cursor) {
            glm::vec2 delta = now_pos - mouse_pos;
            mouse_pos = now_pos;
            under_cursor->fire_event(&node::on_mouse_move, *under_cursor, mouse_pos, delta);
        }
        return true;
    });
    static node* dragging = nullptr;
    static glm::vec2 drag_mouse_pos;
    static glm::vec2 drag_offset;
    on_click.connect([&](node& n, int button, int action, int mods) {
        node* window = &n;
        if (window->parent && window->parent != this) {
            window = window->parent;
        }
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                dragging = window;
                drag_mouse_pos = mouse_pos;
                drag_offset = window->offset;
                auto dragging_it = std::find_if(children.begin(), children.end(), [&](auto& sp) { return sp.get() == dragging; });
                if (dragging_it != children.end()) {
                    std::rotate(dragging_it, dragging_it + 1, children.end());
                }
            } else if (action == GLFW_RELEASE) {
                dragging = nullptr;
            }
        }
        return false;
    });
    on_mouse_move.connect([&](node&, glm::vec2 pos, glm::vec2 dpos) {
        if (dragging) {
            dragging->offset = drag_offset + pos - drag_mouse_pos;
        }
        return false;
    });

    size = {win.width(), win.height()};
    bg_colour = {0.0f, 0.0f, 0.0f, 0.0f};
}

bool te::ui::root::capture_input() {
    return false;
}

void te::ui::root::render(te::sim& sim) {
    cursor = glm::vec2{0.0, 0.0};
    lpa = cursor;
    traverse_dfs([&](node& n, glm::vec2 tl, glm::vec2 size) {
        if (!n.bg_image.empty()) {
            auto& tex = assets.lazy_load<te::gl::texture2d>(n.bg_image);
            canvas.image(tex, tl, n.size, n.bg_tl, glm::abs(n.bg_br - n.bg_tl), glm::vec4{1.0});
        } else {
            canvas.rect(tl, n.size, n.bg_colour);
        }
        if (!n.text_str.empty()) {
            // TODO: don't redo work every frame (including allocating)
            auto para = make_paragraph(text_align::left, n.size.x, 1.0, n.font_, n.text_str);
            for (auto& run : para.runs) {
                for (auto& glyph : run.run.glyphs) {
                    //TODO: what is this 12 doing here?
                    canvas.image(*glyph.texture, tl + glm::vec2{0.0, 12.0} + run.offset + glyph.offset, n.font_.colour);
                }
            }
        }
    }, *this);
    canvas.render();
}

void te::ui::node::text(std::string str) {
    text_str = str;
}

te::ui::button::button(te::ui::root& root, int image): pressed{false} {
    bg_image = fmt::format("assets/a_ui,6.{{}}/{:0>3}.png", image);
    auto& image_tex = root.assets.lazy_load<te::gl::texture2d>(bg_image);
    size = glm::vec2{image_tex.width / 2.0f, image_tex.height};
    bg_tl = {0.0, 1.0};
    bg_br = {0.5, 0.0};
    root.on_click.connect([this](te::ui::node&, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            pressed = false;
        }
        return false;
    });
    on_click.connect([this](te::ui::node&, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                pressed = true;
                bg_tl.x = 0.5f;
                bg_br.x = bg_tl.x + 0.5f;
            } else if (action == GLFW_RELEASE) {
                bg_tl.x = 0.0f;
                bg_br.x = bg_tl.x + 0.5f;
                if (pressed) {
                    pressed = false;
                    on_press(*this);
                }
            }
        }
        return false;
    });
    on_mouse_leave.connect([this](ui::node&) {
        bg_tl.x = 0.0f;
        bg_br.x = bg_tl.x + 0.5f;
        return true;
    });
    on_mouse_enter.connect([this](ui::node&) {
        bg_tl.x = pressed ? 0.5f : 0.0f;
        bg_br.x = bg_tl.x + 0.5f;
        return true;
    });
}

static std::string ui_img(int i) {
    return fmt::format("assets/a_ui,6.{{}}/{:0>3}.png", i);
}

te::ui::generator_window::generator_window(sim& model, root& ui, entt::entity e) {
    generator& gen = model.entities.get<generator>(e);
    named& name = model.entities.get<named>(e);

    bg_image = ui_img(80);
    this->name = "win.frame";
    auto& frame_tex = ui.assets.lazy_load<te::gl::texture2d>(ui_img(80));
    size = glm::vec2{frame_tex.width, frame_tex.height};

    status = this->children.emplace_back(std::make_shared<ui::node>());
    status->parent = this;
    status->offset = {39.0f, 61.0f};
    status->size = {205.0f, 11.0f};
    status->text(gen.active ? "Producing" : "Shut Down");
    status->font_ = font {
        .filename = "Alegreya_Sans_SC/AlegreyaSansSC-Medium.ttf",
        .pts = 7.5,
        .aspect = 1.1,
        .line_height = 1.0,
        .colour = glm::vec4{165.0f, 219.0f, 255.0f, 255.0f} / 255.0f
    };

    auto out_icon_bg = this->children.emplace_back(std::make_shared<ui::node>());
    out_icon_bg->parent = this;
    out_icon_bg->offset = {38.0f, 85.0f};
    out_icon_bg->size = {23.0f, 23.0f};
    out_icon_bg->bg_colour = {1.0f, 1.0f, 1.0f, 1.0f};

    auto out_icon = this->children.emplace_back(std::make_shared<ui::node>());
    out_icon->parent = this;
    out_icon->offset = {39.0f, 86.0f};
    out_icon->size = {21.0f, 21.0f};
    out_icon->bg_image = model.entities.get<render_tex>(gen.output).filename;

    auto out_label = this->children.emplace_back(std::make_shared<ui::node>());
    out_label->parent = this;
    out_label->offset = {70.0f, 91.0f};
    out_label->size = {154.0f, 14.0f};
    out_label->text(model.entities.get<named>(gen.output).name);
    out_label->font_ = font {
        .filename = "Alegreya_Sans_SC/AlegreyaSansSC-Medium.ttf",
        .pts = 7.5,
        .aspect = 1.1,
        .line_height = 1.0,
        .colour = glm::vec4{255.0f, 195.0f, 66.9f, 255.0f} / 255.0f
    };

    auto progress_bar_bg = this->children.emplace_back(std::make_shared<ui::node>());
    progress_bar_bg->parent = this;
    progress_bar_bg->offset = {33.0f, 122.0f};
    progress_bar_bg->bg_image = ui_img(116);
    progress_bar_bg->size = {191.0f, 10.0f};

    progress_bar = this->children.emplace_back(std::make_shared<ui::node>());
    progress_bar->parent = this;
    progress_bar->offset = {34.0f, 123.0f};
    progress_bar->size = {0.4f * 189.0f, 8.0f};
    progress_bar->bg_colour = glm::vec4{57.0f, 158.0f, 222.0f, 255.0f} / 255.0f;

    auto close = std::make_shared<ui::button>(ui, 41);
    this->children.push_back(close);
    close->parent = this;
    close->name = "win.close";
    close->offset = {226, 12};
    close->on_press.connect([&ui, this](ui::button&) {
        auto newend = std::remove_if (
            ui.children.begin(), ui.children.end(),
            [this](std::shared_ptr<node>& sp){ return sp.get() == this; }
        );
        ui.children.erase(newend);
    });
}

void te::ui::generator_window::update(sim& model, root& ui, entt::entity e) {
    generator& gen = model.entities.get<generator>(e);
    progress_bar->size.x = 189.0 * std::min(1.0, gen.progress);
    status->text(gen.active ? "Producing" : "Shut Down");
    status->font_.colour = gen.active
        ? glm::vec4{165.0f, 219.0f, 255.0f, 255.0f} / 255.0f
        : glm::vec4{222.0f,  93.0f,   0.0f, 255.0f} / 255.0f;
}
