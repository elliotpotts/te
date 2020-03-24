#ifndef TE_CONTROLLER_HPP_INCLUDED
#define TE_CONTROLLER_HPP_INCLUDED

#include <te/sim.hpp>
#include <te/net.hpp>
#include <chrono>
#include <memory>
#include <vector>

namespace te {
    namespace act {
        struct build {
            unsigned family;
            entt::entity proto;
            glm::vec2 position;
        };
    }
    
    struct controller {
        std::default_random_engine rengine;
        te::sim& model;
        std::chrono::time_point<std::chrono::high_resolution_clock> last_tick;
        int game_time = 0;
        std::vector<std::unique_ptr<te::peer>> actors;

        controller(te::sim& model);
        void start();
        void poll();

        void generate_map();

        // Apply an action
        void operator()(const te::act::build&);
    };
}

#endif
