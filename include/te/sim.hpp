#ifndef TE_SIM_HPP_INCLUDED
#define TE_SIM_HPP_INCLUDED

#include <string>
#include <entt/entity/view.hpp>

namespace te {
    // Blueprints
    struct commodity {
        std::string name;
        double base_price;
    };
    
    struct site_blueprint {
        std::string name;
        glm::ivec2 dimensions;
    };
    
    struct transport_method {
        std::string name;
    };

    // Sim
    struct site {
        glm::ivec2 position;
        float rotation;
    };

    struct demander {
        std::unordered_map<entt::entity, double> demand;
    };
    
    struct generator {
        entt::entity output;
        double rate;
        double progress = 0.0;
        int stock = 0;
        int max_stock = 4;
    };

    struct market {
        std::unordered_map<entt::entity, double> prices;
        std::unordered_map<entt::entity, int> stock;
        std::unordered_map<entt::entity, double> demand;
        float radius = 5.0f;
    };
    
    struct depot {
        std::vector<transport_method*> allows;
    };

    // TODO: probably not like this...
    struct pathway {
    };

}

#endif
