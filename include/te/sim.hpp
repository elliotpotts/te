#ifndef TE_SIM_HPP_INCLUDED
#define TE_SIM_HPP_INCLUDED

#include <te/util.hpp>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <variant>
#include <glm/vec2.hpp>
#include <entt/entt.hpp>
#include <boost/signals2.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/optional.hpp>

template<typename Ar>
void serialize(Ar& ar, entt::entity x){
    ar(static_cast<ENTT_ID_TYPE>(x));
}

namespace te {
    struct family {
        double balance;
        std::string name;
    };

    struct owned {
        unsigned family_ix;
    };
    template<typename Ar>
    void serialize(Ar& ar, owned& x){
        ar(x.family_ix);
    }

    struct named {
        std::string name;
    };
    template<typename Ar>
    void serialize(Ar& ar, named& x){
        ar(x.name);
    }

    struct described {
        std::string description;
    };
    template<typename Ar>
    void serialize(Ar& ar, described& x) {
        ar(x.description);
    }

    struct price {
        double price;
    };
    template<typename Ar>
    void serialize(Ar& ar, price& x){
        ar(x.price);
    }

    struct footprint {
        glm::vec2 dimensions;
    };
    template<typename  Ar>
    void serialize(Ar& ar, footprint& x){
        ar(x.dimensions);
    }

    struct site {
        glm::vec2 position;
    };
    template<typename Ar>
    void serialize(Ar& ar, site& x){
        ar(x.position);
    }

    struct dweller {
        //TODO: programmatically represent requirements of living
        // i.e. dwellings need 2 of 3 food types in abundance
    };

    // A demander stores the rate of increase of demand of entities
    struct demander {
        std::unordered_map<entt::entity, double> rate;
    };
    template<typename Ar>
    void serialize(Ar& ar, demander& x){
        ar(x.rate);
    }

    // A trader stores the current demand of entities
    struct trader {
        // the index of the family this trader works for
        unsigned family_ix;
        // +ve bid = buying
        // -ve bid = selling
        std::unordered_map<entt::entity, double> bid;
        double balance = 0.0;
    };
    template<typename Ar>
    void serialize(Ar& ar, trader& x){
        ar(x.family_ix, x.bid, x.balance);
    }

    struct generator {
        bool active;
        entt::entity output;
        double rate;
        double progress = 0.0;
    };
    template<typename Ar>
    void serialize(Ar& ar, generator& x){
        ar(x.output, x.rate, x.progress);
    }

    struct producer {
        std::unordered_map<entt::entity, double> inputs;
        std::unordered_map<entt::entity, double> outputs;
        double rate;
        bool producing = false;
        double progress = 0.0;
    };
    template<typename Ar>
    void serialize(Ar& ar, producer& x){
        ar(x.inputs, x.outputs, x.rate, x.producing, x.progress);
    }

    struct inventory {
        std::unordered_map<entt::entity, int> stock;
    };
    template<typename Ar>
    void serialize(Ar& ar, inventory& x){
        ar(x.stock);
    }

    struct market {
        std::unordered_map<entt::entity, double> prices;
        std::unordered_map<entt::entity, double> demand;
        entt::entity commons;
        std::vector<entt::entity> trading;
        double radius = 5.0f;
        int population = 0;
        double growth_rate = 0.001;
        double growth = 0.0;
    };
    template<typename Ar>
    void serialize(Ar& ar, market& x){
        ar(x.prices, x.demand, x.commons, x.trading, x.radius, x.population, x.growth_rate, x.growth);
    }

    struct stop {
        entt::entity where;
        std::unordered_map<entt::entity, int> leave_with;
    };
    template<typename Ar>
    void serialize(Ar& ar, stop& x){
        ar(x.where, x.leave_with);
    }

    struct route {
        std::string name;
        std::vector<stop> stops;
    };
    template<typename Ar>
    void serialize(Ar& ar, route& x){
        ar(x.name, x.stops);
    }

    struct merchant_activity {
        bool trading;
        stop next;
    };
    template<typename Ar>
    void serialize(Ar& ar, merchant_activity& x){
        ar(x.trading, x.next);
    }

    struct merchant {
        std::optional<te::route> route;
        std::size_t next_stop_ix = 0;
    };
    template<typename Ar>
    void serialize(Ar& ar, merchant& x){
        ar(x.route, x.next_stop_ix);
    }

    struct sim {
        std::default_random_engine rengine;

        entt::registry entities;
        std::vector<family> families;
        std::vector<entt::entity> commodities;
        std::vector<entt::entity> blueprints;
        std::vector<route> routes;
        entt::entity merchant_blueprint;

        const int map_width = 40;
        const int map_height = 40;
        std::unordered_map<glm::ivec2, entt::entity> grid;
        glm::vec2 snap(glm::vec2 pos, glm::vec2 print) const;

        sim(unsigned seed);

        void load_commodities();
        void init_blueprints();
        void generate_map();

        std::vector<entt::entity> new_entities;
        entt::entity make_net_entity(unsigned family);

        // total units wanting to be sold
        int market_stock(entt::entity market_e, entt::entity commodity_e);
        int market_demand(entt::entity market_e, entt::entity commodity_e);

        market* market_at(glm::vec2 x);
        bool in_market(const site& question_site, const site& market_site, const market& the_market) const;

        void merchant_embark(entt::entity merchant, const route& route);
        std::optional<merchant_activity> merchant_status(entt::entity merchant);

        bool can_place(entt::entity entity, glm::vec2 where);
        std::optional<entt::entity> try_place(unsigned owner, entt::entity entity, glm::vec2 where);

        bool spawn_dwelling(entt::entity market);
        void spawn(entt::entity proto);

        void tick_merchants(double dt);
        void tick_markets(double dt);
        void tick(double delta_t, bool quiet = true);

        boost::signals2::signal<void()> on_trade;
    };
}

#endif
