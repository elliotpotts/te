#include <te/sim.hpp>
#include <te/app.hpp>
#include <te/server.hpp>
#include <te/client.hpp>
#include <random>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/resource.h>

void graphical_frontend(te::controller& ctrl, te::sim& model) {
    te::app frontend { model, ctrl };
    frontend.run();
}

//TODO: figure out how this is gonna work...
void headless_frontend(te::controller& ctrl, te::sim& model) {
    while (true) {
        ctrl.poll();
    }
}

SteamNetworkingIPAddr init_net() {
    SteamNetworkingIPAddr server_addr;
    server_addr.SetIPv6LocalHost(te::port);
    if (SteamDatagramErrMsg err_msg; !GameNetworkingSockets_Init(nullptr, err_msg)) {
        throw std::runtime_error{fmt::format("GameNetworkingSockets_Init failed: {}", err_msg)};
    }
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, te::network_debug_output);
    return server_addr;
}

int main(const int argc, const char** argv) {    
    spdlog::set_level(spdlog::level::debug);
    // enable core dump to file
    rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    // Add peer
    SteamNetworkingIPAddr server_addr = init_net();
    std::unique_ptr<te::peer> peer;
    if (argc == 1) {
        peer = std::make_unique<te::server>();
    } else if (argc == 2) {
        peer = std::make_unique<te::client>(server_addr);
    }

    // Run
    auto seed = std::random_device{}();
    te::sim model { seed };
    te::controller ctrl { model };
    ctrl.actors.emplace_back(std::move(peer));
    graphical_frontend(ctrl, model);

    // TODO: Put in destructor of something/scope guard
    GameNetworkingSockets_Kill();
    return 0;
}
