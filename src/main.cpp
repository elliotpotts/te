#include <te/sim.hpp>
#include <te/app.hpp>
#include <te/net.hpp>
#include <te/client.hpp>
#include <random>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/resource.h>
#include <guile/3.0/libguile.h>

void network_debug_output(ESteamNetworkingSocketsDebugOutputType type, const char* message) {
    spdlog::debug(message);
    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        throw std::runtime_error("That was a bug! Bye-bye ;)");
    }
}

SteamNetworkingIPAddr init_net() {
    SteamNetworkingIPAddr server_addr;
    server_addr.SetIPv6LocalHost(te::port);
    if (SteamDatagramErrMsg err_msg; !GameNetworkingSockets_Init(nullptr, err_msg)) {
        throw std::runtime_error{fmt::format("GameNetworkingSockets_Init failed: {}", err_msg)};
    }
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, network_debug_output);
    return server_addr;
}

void* guile_main(void* _) {
    spdlog::set_level(spdlog::level::debug);
    // enable core dump to file
    rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    // Create empty model
    auto seed = std::random_device{}();
    te::sim model { seed };

    // Create peer
    SteamNetworkingIPAddr server_addr = init_net();
    {
        te::app frontend { model, server_addr };
        frontend.run();
    }

    // TODO: Put in destructor of something/scope guard
    GameNetworkingSockets_Kill();
    return 0;
}

int main(const int argc, const char** argv) {
    scm_with_guile(&guile_main, nullptr);
}
