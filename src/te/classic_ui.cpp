#include <te/canvas_renderer.hpp>
#include <te/classic_ui.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <te/components.hpp>
#include <scm.hpp>

constexpr glm::vec4 col_white = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr glm::vec4 col_red = glm::vec4{222, 93, 0, 255} / 255.0f;
constexpr glm::vec4 col_blue = glm::vec4{165, 219, 255, 255} / 255.0f;
constexpr glm::vec4 col_lblue = glm::vec4{57, 158, 222, 255} / 255.0f;

te::classic_ui::classic_ui(te::sim& s, te::window& w, te::canvas_renderer& d, te::cache<asset_loader>& r):
    model{&s},
    input_win{&w},
    canvas{&d},
    resources{&r},
    behind {
        .size = {w.width(), w.height()}
    }
    {
}

bool inside_rect(glm::vec2 pt, glm::vec2 tl, glm::vec2 br) {
    return pt.x >= tl.x && pt.x <= br.x && pt.y >= tl.y && pt.y <= br.y;
}

bool te::classic_ui::mouse_inside(glm::vec2 tl, glm::vec2 br) const {
    return cursor_pos.x >= tl.x && cursor_pos.x <= br.x
        && cursor_pos.y >= tl.y && cursor_pos.y <= br.y;
}

void te::classic_ui::input(glm::vec2 o, rect& r) {
    bool mouse_was_inside = inside_rect(prev_cursor_pos, o + r.offset, o + r.offset + r.size);
    bool mouse_is_inside = inside_rect(cursor_pos, o + r.offset, o + r.offset + r.size);
    if (mouse_was_inside && !mouse_is_inside && !leave_absorbed) {
        r.on_mouse_leave();
        leave_absorbed = true;
    }
    if (!mouse_was_inside && mouse_is_inside && !enter_absorbed) {
        r.on_mouse_enter();
        enter_absorbed = true;
    }
    if (mouse_was_inside && !mouse_was_down && mouse_is_inside && mouse_is_down && !down_absorbed) {
        r.click_started = true;
        down_absorbed = true;
        r.on_mouse_down();
    }
    if (mouse_was_inside && mouse_was_down && mouse_is_inside && !mouse_is_down) {
        if (r.click_started && !click_absorbed) {
            click_absorbed = true;
            r.on_click();
        }
        if (!up_absorbed) {
            r.on_mouse_up();
            up_absorbed = true;
        }
    }
    if (mouse_is_inside && !mouse_is_down) {
        r.click_started = false;
    }
}

bool te::classic_ui::input() {
    prev_cursor_pos = cursor_pos;
    mouse_was_down = mouse_is_down;

    glfwGetCursorPos(input_win->hnd.get(), &cursor_pos.x, &cursor_pos.y);
    mouse_is_down = glfwGetMouseButton(input_win->hnd.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    enter_absorbed = false;
    leave_absorbed = false;
    click_absorbed = false;
    down_absorbed = false;
    up_absorbed = false;

    to_close = windows.end();
    to_bring_to_front = windows.end();

    for (auto it = windows.rbegin(); it != windows.rend(); it++) {
        (*it)->input(*this, (*it)->offset);
    }
    input({}, behind);

    if (to_close != windows.end()) {
        windows.erase(to_close);
    }

    if (to_bring_to_front != windows.end()) {
        std::rotate(to_bring_to_front, std::next(to_bring_to_front), windows.end());
    }

    if (dragging) {
        (*std::find_if(windows.begin(), windows.end(), [this](const auto& w){ return w->id == dragging; }))
            ->offset = glm::vec2{cursor_pos} + drag_offset;
    }

    return up_absorbed;
}

void te::classic_ui::draw_ui(glm::vec2 o, rect& r) {
    if (r.colour) {
        canvas->rect(o + r.offset, r.size, *r.colour);
    }
    if (r.fname) {
        auto& tex = resources->lazy_load<te::gl::texture2d>(*r.fname);
        const glm::vec2 tex_size {tex.width, tex.height};
        canvas->image(tex, o + r.offset, tex_size, {1.0, 1.0}, {1.0, 1.0});
    }
}

void te::classic_ui::draw_ui(glm::vec2 o, label& l) {
    canvas->text(l.text, o + l.offset, l.font);
}

void te::classic_ui::show_window(std::unique_ptr<drag_window> w) {
    auto& appended = windows.emplace_back(std::move(w));
    unsigned win_id = appended->id = next_id++;
    appended->close.on_click.connect([this, win_id]() {
        to_close = std::find_if(windows.begin(), windows.end(), [win_id](const auto& w) { return w->id == win_id; });
    });
    appended->frame.on_mouse_down.connect([this, win_id]() {
        dragging = win_id;
        auto me = std::find_if(windows.begin(), windows.end(), [win_id](const auto& w) { return w->id == win_id; });
        to_bring_to_front = me;
        drag_offset = (*me)->offset - glm::vec2{cursor_pos};
        (*me)->frame.on_mouse_up.connect_extended([this](const boost::signals2::connection& up_conn) {
            dragging.reset();
            up_conn.disconnect();
        });
    });
}

void te::classic_ui::render() {
    for (auto& win : windows) {
        win->update(*model);
        win->draw_ui(*this, win->offset);
    }
    canvas->render();
}

te::classic_ui::console::console() {
   lines.emplace_back();
    offset = {20, 20};
    frame = rect {
        .colour = glm::vec4{0, 0, 0, 1},
        .size {300, 300}
    };
    close = button {
        {
            .name = "close",
            .fname = "assets/a_ui,6.{}/041.png",
            .offset = glm::vec2{260, 10},
            .size = {28, 27}
        }
    };
}

void te::classic_ui::console::input(te::classic_ui& ui, glm::vec2 o) {
    //drag_window::input(ui, o);
}

void te::classic_ui::console::update(te::sim&) {
}

void te::classic_ui::console::draw_ui(classic_ui&, glm::vec2 o) {
}
void te::classic_ui::drag_window::draw_ui(classic_ui&, glm::vec2 o) {
}

