#include <te/canvas_renderer.hpp>
#include <te/classic_ui.hpp>
#include <te/util.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <te/components.hpp>
#include <scm.hpp>

te::ui::node* img(te::ui::node& n, std::string fname, glm::vec2 offset, bool visible) {
    n.image = fname;
    n.size = glm::vec2{1, 1};
    n.colour = glm::vec4{1};
    n.offset = offset;
    n.visible = visible;
    return &n;
}

/* a node tree is the compilation target of the gui compiler!
   that's it...
   <img> produces a node who is sized to the img size
   <div> produces a node who is size accoridng to text, and background-image, and text node

   we must see the model (img, div, button, etc.) as separate (and higher level) than the nodes themselves
 */
te::ui::button::button(te::ui::node& root, const char* i, bool value) : pressed{value} {
    n = img(root.children.emplace_back(), i, {0, 0}, true);
    n->size = {0.5, 1};
    n->image_size = {0.5, 1};
}
void te::ui::button::update() {
    if (pressed) {
        n->image_offset = {0.5, 0};
    } else {
        n->image_offset = {0, 0};
    }
}

te::ui::uui::uui(te::ui::node& root) {
    top_bar = img(root.children.emplace_back(), "168", {0, 0}, true);

    panel_open = nullptr;
    panel_button = nullptr;

    construction_panel = img(root.children.emplace_back(), "002", {0, 20}, false);
    cons_0_back = img(construction_panel->children.emplace_back(), "001", {13, 65}, true);
    cons_0_text = &construction_panel->children.emplace_back();
    cons_0_text->text = "PATHWAYS";
    cons_0_text->text_font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 8.0, 2.0, {1.0, 1.0, 1.0, 1.0}};
    cons_0_text->offset = {13, 65 + 16};
    cons_0_text->visible = true;

    roster_panel = img(root.children.emplace_back(), "011", {0, 20}, false);
    orders_panel = img(root.children.emplace_back(), "013", {0, 20}, false);
    routes_panel = img(root.children.emplace_back(), "089", {0, 20}, false);
    tech_panel = img(root.children.emplace_back(), "136", {0, 20}, false);

    bottom_bar = img(root.children.emplace_back(), "169", {0, 714}, true);
    bottom_bar_text = &bottom_bar->children.emplace_back();
    bottom_bar_text->id = "bottom_bar_text";
    bottom_bar_text->visible = true;
    bottom_bar_text->size = {485, 28};
    bottom_bar_text->offset = {271, 13};
    bottom_bar_text->colour = glm::vec4{0.0};
    bottom_bar_text->text = "foo";
    bottom_bar_text->text_font = {"Alegreya_Sans_SC/AlegreyaSansSC-Bold.ttf", 8.0, 2.0, {1.0, 1.0, 1.0, 1.0}};

    roster_button = new button {root, "176", false};
    roster_button->n->offset = {28, 14 + 714};
    roster_button->n->on_mouse_down.connect([this]() { toggle_button(roster_button, roster_panel); });
    routes_button = new button {root, "177", false};
    routes_button->n->offset = {84, 14 + 714};
    routes_button->n->on_mouse_down.connect([this]() { toggle_button(routes_button, routes_panel); });
    construction_button = new button {root, "178", false};
    construction_button->n->offset = {141, 14 + 714};
    construction_button->n->on_mouse_down.connect([this]() { toggle_button(construction_button, construction_panel); });
    tech_button = new button {root, "179", false};
    tech_button->n->offset = {198, 14 + 714};
    tech_button->n->on_mouse_down.connect([this]() { toggle_button(tech_button, tech_panel); });
};

void te::ui::uui::toggle_button(button* btn, te::ui::node* pnl) {
    if (panel_open) {
        panel_open->visible = false;
        panel_button->pressed = false;
        panel_button->update();
    }
    if (panel_open == pnl) {
        panel_open = nullptr;
        panel_button = nullptr;
    } else {
        panel_open = pnl;
        panel_open->visible = !panel_open->visible;
        panel_button = btn;
        panel_button->pressed = !panel_button->pressed;
        panel_button->update();
    }
};

te::ui::node::node() : visible{true}, size{0.0}, offset{0.0}, colour{0.0}, image_size{1, 1}, image_offset{0,0} {
}

bool inside_rect(glm::vec2 pt, glm::vec2 tl, glm::vec2 br) {
    return pt.x >= tl.x && pt.x <= br.x && pt.y >= tl.y && pt.y <= br.y;
}

glm::vec2 screen_size(const te::ui::node& n) {
}

void te::ui::root::cursor_move(double x, double y) {
    node* const old_over = over;
    over = &dom;

    glm::vec2 origin {0, 0};
    using child_it_t = std::list<node>::iterator;
    child_it_t child_it = dom.children.begin();
    child_it_t child_it_end = dom.children.end();
    while (child_it != child_it_end) {
        const auto& tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}.png", child_it->image));
        const glm::vec2 tex_size = glm::vec2{tex.width, tex.height};
        const auto screen_size = tex_size * child_it->size;
        const glm::vec2 tl = origin + child_it->offset;
        const glm::vec2 br = origin + child_it->offset + screen_size;
        if (inside_rect({x, y}, tl, br)) {
            over = std::to_address(child_it);
        }
        child_it++;
    }
    if (old_over && old_over != over) {
        old_over->on_mouse_leave();
    }
    if (old_over != over) {
        over->on_mouse_enter();
    }
}

te::ui::root::root(te::sim& s, te::window& w, te::canvas_renderer& d, te::cache<asset_loader>& r):
    input_win{&w}, canvas{&d}, resources{&r}, dom{}, focused{nullptr}, clicking{nullptr}, over{nullptr}
{
    w.on_cursor_move.connect([&](double x, double y) { cursor_move(x, y); });
    w.on_mouse_button.connect([&](int button, int action, int mods) {
        if (action == GLFW_PRESS && over) {
            over->on_mouse_down();
            clicking = over;
        } else if (action == GLFW_RELEASE && over) {
            over->on_mouse_up();
            if (over == clicking) {
                over->on_click();
                clicking = nullptr;
            }
        }
    });

    dom.size = glm::vec2{ w.width(), w.height() };
    foo = new uui { dom };
}

bool te::ui::root::input() {
    return false;
}

void te::ui::root::render(glm::vec2 origin, te::ui::node& n) {
    if (n.colour.a != 0.0) {
        canvas->rect(origin, n.size, n.colour);
    }
    if (!n.image.empty()) {
        auto& tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}.png", n.image));
        const glm::vec2 tex_size = glm::vec2{tex.width, tex.height};
        glm::vec4 colour = n.colour;
        if (&n == over) {
            colour -= glm::vec4{0, .1, .1, 0};
        }
        canvas->image(tex, origin, tex_size * n.size, n.image_offset, n.image_size, colour);
    }
    if (!n.text.empty()) {
        canvas->text(n.text, origin, n.text_font);
    }
    for (auto& child : n.children) {
        if (child.visible) {
            render(origin + child.offset, child);
        }
    }
}

void te::ui::root::render() {
    render(glm::vec2{0.0, 0.0}, dom);
    canvas->render();
}
