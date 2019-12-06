#include <te/sim.hpp>
#include <te/app.hpp>
#include <random>

int main(void) {
    spdlog::set_level(spdlog::level::debug);
    std::random_device seed_device;
    te::sim model { seed_device() };
    te::app frontend { model, seed_device() };
    frontend.run();
    return 0;
}
