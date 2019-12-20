#include <te/sim.hpp>
#include <te/app.hpp>
#include <random>
#include <sys/resource.h>

/* We have a few things happening:
 *   rendering
 *   networing
 *   simulation
 * These will run at different framerates
 */

int main(void) {
    // enable core dump to file
    rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    // run program
    spdlog::set_level(spdlog::level::debug);
    std::random_device seed_device;
    te::sim model { seed_device() };
    te::app frontend { model, seed_device() };
    frontend.run();
    return 0;
}
