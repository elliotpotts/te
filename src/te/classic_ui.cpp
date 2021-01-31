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

struct button {
    bool pressed;
    te::ui::node* node;
    button(te::ui::node& root, bool value) : pressed{value} {
        node = img(root.children.emplace_back(), "176.png", {0, 0}, true);
        node->size = {0.5, 1};
        node->image_size = {0.5, 1};
        node->on_mouse_down.connect([this]() {
            pressed = true;
            node->image_offset = {0.5, 0};
        });
        node->on_mouse_up.connect([&]() {
            pressed = false;
            node->image_offset = {0, 0};
        });
    }
};

struct uui {
    te::ui::node* top_bar;

    te::ui::node* construction_panel;
    te::ui::node* roster_panel;
    te::ui::node* orders_panel;
    te::ui::node* route_panel;
    te::ui::node* tech_panel;

    button construction_button;
    button roster_button;

    te::ui::node* bottom_bar;

    uui(te::ui::node& root) : construction_button{root, false}, roster_button{root, false} {
        construction_button.node->offset = {40, 0};
        /*top_bar = img(root.children.emplace_back(), "168.png", {0, 0}, true);
        construction_panel = img(root.children.emplace_back(), "002.png", {0, 20}, false);
        roster_panel = img(root.children.emplace_back(), "011.png", {0, 20}, false);
        orders_panel = img(root.children.emplace_back(), "013.png", {0, 20}, false);
        route_panel = img(root.children.emplace_back(), "089.png", {0, 20}, false);
        tech_panel = img(root.children.emplace_back(), "136.png", {0, 20}, true);
        bottom_bar = img(root.children.emplace_back(), "169.png", {0, 714}, true);*/
    };
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
        const auto& tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}", child_it->image));
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
    input_win{&w}, canvas{&d}, resources{&r}, dom{}, over{nullptr}
{
    w.on_cursor_move.connect([&](double x, double y) { cursor_move(x, y); });
    w.on_mouse_button.connect([&](int button, int action, int mods) {
        if (action == GLFW_PRESS && over) {
            over->on_mouse_down();
        } else if (action == GLFW_RELEASE && over) {
            over->on_mouse_up();
        }
    });

    dom.size = glm::vec2{ w.width(), w.height() };
    new uui { dom };
}

bool te::ui::root::input() {
    return true;
}

void te::ui::root::render(glm::vec2 origin, te::ui::node& n) {
    if (n.colour.a != 0.0) {
        canvas->rect(origin, n.size, n.colour);
    }
    if (!n.image.empty()) {
        auto& tex = resources->lazy_load<te::gl::texture2d>(fmt::format("assets/a_ui,6.{{}}/{}", n.image));
        const glm::vec2 tex_size = glm::vec2{tex.width, tex.height};
        glm::vec4 colour = n.colour;
        if (&n == over) {
            colour -= glm::vec4{0, .7, .7, 0};
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
