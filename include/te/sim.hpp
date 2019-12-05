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
        std::unordered_map<entt::registry::entity_type, double> demand;
    };
    
    struct generator {
        entt::registry::entity_type output;
        double rate;
        double progress = 0.0;
        int stock = 0;
        int max_stock = 4;
    };
    
    struct bid {
        entt::registry::entity_type good;
        double price;
    };

    struct market {
        std::vector<bid> bids;
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
