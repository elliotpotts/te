#include <te/sim.hpp>

te::sim::sim(unsigned int seed) : rengine { seed } {
    init_blueprints();
    generate_map();
}

void te::sim::init_blueprints() {
    // Commodities
    auto wheat = entities.create();
    entities.assign<commodity>(wheat, "Wheat", 15.0f);
    entities.assign<render_tex>(wheat, "media/wheat.png");

    auto barley = entities.create();
    entities.assign<commodity>(barley, "Barley", 10.0f);
    entities.assign<render_tex>(barley, "media/wheat.png");

    std::unordered_map<entt::entity, double> base_market_prices;
    entities.view<commodity>().each([&](const entt::entity e, commodity& comm) {
                                        base_market_prices[e] = comm.base_price;
                                    });

    // Buildings
    auto wheat_field = site_blueprints.emplace_back(entities.create());
    entities.assign<site_blueprint>(wheat_field, "Wheat Field", glm::vec2{2.0f,2.0f});
    entities.assign<generator>(wheat_field, wheat, 0.004);
    entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
    entities.assign<pickable>(wheat_field);

    auto barley_field = site_blueprints.emplace_back(entities.create());
    entities.assign<site_blueprint>(barley_field, "Barley Field", glm::vec2{2.0f,2.0f});
    entities.assign<generator>(barley_field, barley, 0.005);
    entities.assign<render_mesh>(barley_field, "media/wheat.glb");
    entities.assign<pickable>(barley_field);

    auto dwelling = site_blueprints.emplace_back(entities.create());
    entities.assign<site_blueprint>(dwelling, "Dwelling", glm::vec2{1.0f,1.0f});
    demander& dwelling_demands = entities.assign<demander>(dwelling);
    dwelling_demands.demand[wheat] = 0.0005f;
    dwelling_demands.demand[barley] = 0.0004f;
    entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
    entities.assign<pickable>(dwelling);

    auto market = site_blueprints.emplace_back(entities.create());
    entities.assign<site_blueprint>(market, "Market", glm::vec2{2.0f,2.0f});
    entities.assign<te::market>(market, base_market_prices);
    entities.assign<render_mesh>(market, "media/market.glb");
    entities.assign<pickable>(market);
}

void te::sim::generate_map() {
    std::discrete_distribution<std::size_t> select_blueprint {5, 3, 50, 2};
    for (int i = 0; i < 200; i++) spawn(site_blueprints[select_blueprint(rengine)]);
}

te::market* te::sim::market_at(glm::vec2 x) {
    auto markets = entities.view<market, site>();
    auto it = std::find_if (
        markets.begin(),
        markets.end(),
        [&](auto& e) {
            auto& market_site = markets.get<site>(e);
            auto& the_market = markets.get<market>(e);
            return glm::length(glm::vec2{market_site.position - x}) <= the_market.radius;
        }
    );
    if (it != markets.end()) {
        return &markets.get<market>(*it);
    } else {
        return nullptr;
    };
}

bool te::sim::in_market(const site& question_site, const site& market_site, const market& the_market) const {
    return glm::length(glm::vec2{question_site.position - market_site.position}) <= the_market.radius;
}
bool te::sim::can_place(entt::entity entity, glm::vec2 where) {
    {
        auto& blueprint = entities.get<site_blueprint>(entity);
        for (int x = 0; x < blueprint.dimensions.x; x++)
            for (int y = 0; y < blueprint.dimensions.y; y++)
                if (grid.find({where.x + x, where.y + y}) != grid.end())
                    return false;
    }
    if (auto maybe_market = entities.try_get<market>(entity); maybe_market) {
        auto other_markets = entities.view<site, market>();
        const bool conflict = std::any_of (
            other_markets.begin(),
            other_markets.end(),
            [&] (auto other) {
                const auto& [other_site, other_market] = other_markets.get<site, market>(other);
                return glm::length(glm::vec2{where - other_site.position})
                    <= (maybe_market->radius + other_market.radius);
            }
        );
        if (conflict) return false;
    }
    return true;
}

void te::sim::place(entt::entity proto, glm::vec2 where) {
    auto instantiated = entities.create(proto, entities);
    entities.assign<site>(instantiated, where);
    auto& print = entities.get<site_blueprint>(instantiated);
    for (int x = 0; x < print.dimensions.x; x++) {
        for (int y = 0; y < print.dimensions.y; y++) {
            grid[{where.x + x, where.y + y}] = instantiated;
        }
    }
}

void te::sim::spawn(entt::entity proto) {
    auto print = entities.get<site_blueprint>(proto);
    std::uniform_int_distribution select_x_pos {0, map_width - static_cast<int>(print.dimensions.x)};
    std::uniform_int_distribution select_y_pos {0, map_height - static_cast<int>(print.dimensions.y)};
try_again:
    glm::vec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
    if (!can_place(proto, map_pos))
        goto try_again;
    place(proto, map_pos);
}

void te::sim::tick() {
    entities.view<market, site>().each (
        [&](auto& market, auto& market_site) {
            entities.view<generator, site>().each (
                [&](auto& generator, auto& generator_site) {
                    if (in_market(generator_site, market_site, market)) {
                        if (generator.progress < 1.0f) {
                            generator.progress += generator.rate;
                        } else if (generator.progress >= 1.0f && market.stock[generator.output] < 10) {
                            market.stock[generator.output]++;
                            generator.progress -= 1.0f;
                        }
                    }
                }
            );
            entities.view<demander, site>().each (
                [&](auto& demander, auto& demander_site) {
                    if (in_market(demander_site, market_site, market)) {
                        for (auto [commodity_e, commodity_demand] : demander.demand) {
                            market.demand[commodity_e] += commodity_demand;
                        }
                    }
                }
            );
            entities.view<commodity>().each (
                [&](entt::entity commodity_e, auto& commodity) {
                    int curr_stock = market.stock[commodity_e];
                    double curr_demand = market.demand[commodity_e];
                    int demand_units = static_cast<int>(curr_demand);
                    int movement = std::min(curr_stock, demand_units);
                    market.stock[commodity_e] = curr_stock - movement;
                    market.demand[commodity_e] = curr_demand - static_cast<double>(movement);
                }
            );
        }
    );
}
