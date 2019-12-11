#ifndef TE_SIM_HPP_INCLUDED
#define TE_SIM_HPP_INCLUDED

#include <te/util.hpp>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <glm/vec2.hpp>
#include <entt/entt.hpp>

namespace te {
    // Blueprints
    struct commodity {
        std::string name;
        double base_price;
    };
    
    struct site_blueprint {
        std::string name;
        glm::vec2 dimensions;
    };
    
    struct transport_method {
        std::string name;
    };

    // Sim
    // location of the centre of the entity
    struct site {
        glm::vec2 position;
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

    struct stop {
        entt::entity where;
        std::unordered_map<entt::entity, int> leave_with;
    };
    
    struct route {
        std::vector<stop> stops;
    };

    struct merchant {
        te::route route;
        std::size_t last_stop;
        std::unordered_map<entt::entity, int> inventory;
    };

    struct sim {
        std::default_random_engine rengine;
        entt::registry entities;
        std::vector<entt::entity> site_blueprints;
        entt::entity merchant_blueprint;

        const int map_width = 40;
        const int map_height = 40;
        std::unordered_map<glm::ivec2, entt::entity> grid;

        sim(unsigned seed);

        void init_blueprints();
        void generate_map();

        market* market_at(glm::vec2 x);
        bool in_market(const site& question_site, const site& market_site, const market& the_market) const;
        bool can_place(entt::entity entity, glm::vec2 where);

        void place(entt::entity proto, glm::vec2 where);
        void spawn(entt::entity proto);
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
