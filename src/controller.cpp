#include <te/controller.hpp>
#include <spdlog/spdlog.h>

te::controller::controller(te::sim& model) : model{model} {
}

void te::controller::start() {
    last_tick = std::chrono::high_resolution_clock::now();
}

void te::controller::poll() {
    auto now = std::chrono::high_resolution_clock::now();
    auto since_last_tick = now - last_tick;
    static auto tick_t = std::chrono::milliseconds{250};
    if (since_last_tick >= tick_t) {
        last_tick += tick_t;
        model.tick(0.25); //TODO: figure out how tf durations work
        game_time++;
    }
}

void te::controller::generate_map() {
    std::discrete_distribution<std::size_t> select_blueprint {7, 7, 2, 0, 2};
    for (int i = 0; i < 40; i++) model.spawn(model.blueprints[select_blueprint(rengine)]);
    // create a roaming merchant
    auto merchant_e = model.entities.create();
    model.entities.assign<named>(merchant_e, "Nebuchadnezzar");
    model.entities.assign<site>(merchant_e, glm::vec2{0.0f, 0.0f});
    model.entities.assign<footprint>(merchant_e, glm::vec2{1.0f, 1.0f});
    model.entities.assign<render_mesh>(merchant_e, "assets/merchant.glb");
    model.entities.assign<pickable>(merchant_e);
    model.entities.assign<trader>(merchant_e, 1u);
    model.entities.assign<inventory>(merchant_e);
    model.entities.assign<merchant>(merchant_e, std::nullopt);
}

void te::controller::operator()(const act::build& act) {
    auto placed = model.try_place(act.proto, act.position);
    if (placed) {
        model.entities.assign<te::owned>(*placed, act.family);
    } else {
        //somehow send failure message
    }
}
