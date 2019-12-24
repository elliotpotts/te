#include <te/sim.hpp>
#include <te/app.hpp>
#include <random>
#include <spdlog/spdlog.h>
#include <sys/resource.h>

int main(const int argc, const char** argv) {
    spdlog::set_level(spdlog::level::debug);
    // enable core dump to file
    rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    auto seed = std::random_device{}();
    te::sim model { seed };
    te::app frontend { model, seed };
    frontend.run();
    return 0;
}
