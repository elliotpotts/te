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
    struct named {
        std::string name;
    };
    
    struct commodity {
        double base_price;
    };
    
    struct site_blueprint {
        glm::vec2 dimensions;
    };

    struct site {
        glm::vec2 position;
    };

    struct dweller {
        //TODO: programmatically represent requirements of living
        // i.e. dwellings need 2 of 3 food types in abundance
        bool dummy;
    };

    // A demander stores the rate of increase of demand of entities
    struct demander {
        std::unordered_map<entt::entity, double> rate;
    };

    // A trader stores the current demand of entities
    struct trader {
        // +ve bid = buying
        // -ve bid = selling
        std::unordered_map<entt::entity, double> bid;
    };
    
    struct generator {
        entt::entity output;
        double rate;
        double progress = 0.0;
    };

    struct inventory {
        std::unordered_map<entt::entity, int> stock;
    };

    struct market {
        std::unordered_map<entt::entity, double> prices;
        std::unordered_map<entt::entity, double> demand;
        double radius = 5.0f;
        int population = 0;
        double growth_rate = 0.001;
        double growth = 0.0;
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
        bool spawn_dwelling(entt::entity market);

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
