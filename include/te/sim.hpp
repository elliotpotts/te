#ifndef TE_SIM_HPP_INCLUDED
#define TE_SIM_HPP_INCLUDED

#include <entt/entt.hpp>
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <random>
#include <te/util.hpp>

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

    struct sim {
        std::default_random_engine rengine;
        entt::registry entities;
        std::vector<entt::registry::entity_type> site_blueprints;

        const int map_width = 40;
        const int map_height = 40;
        std::unordered_map<glm::ivec2, entt::registry::entity_type> map;

        sim(unsigned seed);

        void init_blueprints();
        void generate_map();

        market* market_at(glm::ivec2 x);
        bool in_market(const site& question_site, const site& market_site, const market& the_market) const;
        bool can_place(entt::registry::entity_type entity, glm::ivec2 where);

        void place(entt::registry::entity_type proto, glm::ivec2 where);
        void spawn(entt::registry::entity_type proto);
        void tick();
    };

    //TOOD: put these somewhere else
    // Client Components
    struct render_tex {
        std::string filename;
    };
    struct render_mesh {
        std::string filename;
    };
    struct pickable {
    };
}

#endif
