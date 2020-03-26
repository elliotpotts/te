#include <te/sim.hpp>
#include <te/app.hpp>
#include <te/net.hpp>
#include <te/server.hpp>
#include <te/client.hpp>
#include <random>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/resource.h>

te::peer::~peer() {
}

SteamNetworkingIPAddr init_net() {
    SteamNetworkingIPAddr server_addr;
    server_addr.SetIPv6LocalHost(te::port);
    if (SteamDatagramErrMsg err_msg; !GameNetworkingSockets_Init(nullptr, err_msg)) {
        throw std::runtime_error{fmt::format("GameNetworkingSockets_Init failed: {}", err_msg)};
    }
    //SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, te::network_debug_output);
    ISteamNetworkingUtils* utils = SteamNetworkingUtils();
    utils->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, te::network_debug_output);
    return server_addr;
}

int main(const int argc, const char** argv) {    
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
        std::unique_ptr<te::peer> peer;
        unsigned family_ix;
        if (argc == 1) {
            peer = std::make_unique<te::server>(model);
            static_cast<te::server*>(peer.get())->listen(te::port);
            family_ix = 1;
            model.generate_map();
        } else if (argc == 2) {
            peer = std::make_unique<te::client>(server_addr, model);
            family_ix = 2;
        }
        
        // Create frontend & run
        te::app frontend { model, *peer, family_ix };
        frontend.run();
    }

    // TODO: Put in destructor of something/scope guard
    GameNetworkingSockets_Kill();
    return 0;
}
