#include <te/sim.hpp>
#include <spdlog/spdlog.h>

te::sim::sim(unsigned int seed) : rengine { seed } {
    init_blueprints();
    generate_map();
}

void te::sim::init_blueprints() {
    // Commodities
    auto wheat = entities.create();
    entities.assign<named>(wheat, "Wheat");
    entities.assign<commodity>(wheat, 15.0f);
    entities.assign<render_tex>(wheat, "media/wheat.png");

    auto barley = entities.create();
    entities.assign<named>(barley, "Barley");
    entities.assign<commodity>(barley, 10.0f);
    entities.assign<render_tex>(barley, "media/wheat.png");

    std::unordered_map<entt::entity, double> base_market_prices;
    entities.view<commodity>().each([&](const entt::entity e, commodity& comm) {
                                        base_market_prices[e] = comm.base_price;
                                    });

    // Buildings
    auto wheat_field = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(wheat_field, "Wheat Field");
    entities.assign<site_blueprint>(wheat_field, glm::vec2{2.0f,2.0f});
    entities.assign<generator>(wheat_field, wheat, 0.004);
    entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
    entities.assign<pickable>(wheat_field);

    auto barley_field = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(barley_field, "Barley Field");
    entities.assign<site_blueprint>(barley_field, glm::vec2{2.0f,2.0f});
    entities.assign<generator>(barley_field, barley, 0.005);
    entities.assign<render_mesh>(barley_field, "media/barley.glb");
    entities.assign<pickable>(barley_field);

    auto dwelling = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(dwelling, "Dwelling");
    entities.assign<site_blueprint>(dwelling, glm::vec2{1.0f,1.0f});
    demander& dwelling_demander = entities.assign<demander>(dwelling);
    dwelling_demander.rate[wheat] = 0.0005f;
    dwelling_demander.rate[barley] = 0.0004f;
    entities.assign<trader>(dwelling);
    entities.assign<living>(dwelling);
    entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
    entities.assign<pickable>(dwelling);

    auto market = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(market, "Market");
    entities.assign<site_blueprint>(market, glm::vec2{2.0f,2.0f});
    entities.assign<te::market>(market, base_market_prices);
    entities.assign<inventory>(market);
    entities.assign<render_mesh>(market, "media/market.glb");
    entities.assign<pickable>(market);
}

void te::sim::generate_map() {
    std::discrete_distribution<std::size_t> select_blueprint {7, 7, 0, 1};
    for (int i = 0; i < 100; i++) spawn(site_blueprints[select_blueprint(rengine)]);
    // create a roaming merchant
    auto merchant_e = entities.create();
    entities.assign<named>(merchant_e, "Merchant Nebuchadnezzar");
    entities.assign<site>(merchant_e, glm::vec2{0.0f, 0.0f});
    entities.assign<site_blueprint>(merchant_e, glm::vec2{1.0f, 1.0f});
    entities.assign<render_mesh>(merchant_e, "media/merchant.glb");
    entities.assign<pickable>(merchant_e);
    entities.assign<inventory>(merchant_e);
    std::vector<te::stop> salesman;
    for (auto market : entities.view<site, market>()) {
        salesman.push_back(te::stop { market });
    }
    entities.assign<merchant> (
        merchant_e,
        route { salesman },
        salesman.size() - 1
    );
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

void te::sim::spawn_dwelling(entt::entity market_e) {
    const auto& [market_site, market] = entities.get<site, te::market>(market_e);
    std::uniform_int_distribution select_x_pos {
        static_cast<int>(market_site.position.x - market.radius),
        static_cast<int>(market_site.position.x + market.radius)
    };
    std::uniform_int_distribution select_y_pos {
        static_cast<int>(market_site.position.y - market.radius),
        static_cast<int>(market_site.position.y + market.radius)
    };
    int attempts = 0;
try_again:
    glm::vec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
    if (!can_place(site_blueprints[2], map_pos) && attempts++ >= 5)
        goto try_again;
    place(site_blueprints[2], map_pos);
}

void te::sim::tick() {
    entities.view<merchant, site>().each (
        [&](auto& merchant, auto& merchant_site) {
            std::size_t next_stop_ix = (merchant.last_stop + 1) % merchant.route.stops.size();
            auto next_site = entities.get<site>(merchant.route.stops[next_stop_ix].where);
            auto to_go = next_site.position - merchant_site.position;
            auto distance_to_next_stop = glm::length(to_go);
            // if he's close enough, advance to next part of route and recalculate
            if (distance_to_next_stop <= 2.0f) {
                merchant.last_stop = next_stop_ix;
                next_stop_ix = (next_stop_ix + 1) % merchant.route.stops.size();
                next_site = entities.get<site>(merchant.route.stops[next_stop_ix].where);
                distance_to_next_stop = glm::length(merchant_site.position - next_site.position);
            }
            // move him a bit closer
            merchant_site.position += glm::normalize(to_go) * 0.05f;
        }
    );
    entities.view<market, inventory, site>().each (
        [&](entt::entity market_e, auto& market, auto& inventory, auto& market_site) {
            // advance generators
            entities.view<generator, site>().each (
                [&](auto& generator, auto& generator_site) {
                    if (in_market(generator_site, market_site, market)) {
                        if (generator.progress < 1.0f) {
                            generator.progress += generator.rate;
                        } else if (generator.progress >= 1.0f && inventory.stock[generator.output] < 10) {
                            inventory.stock[generator.output]++;
                            generator.progress -= 1.0f;
                        }
                    }
                }
            );
            // raise trader demands
            entities.view<demander, trader, site>().each (
                [&](auto& demander, auto& trader, auto& demander_site) {
                    if (in_market(demander_site, market_site, market)) {
                        for (auto [commodity_e, demand_rate] : demander.rate) {
                            trader.bid[commodity_e] += demand_rate;
                        }
                    }
                }
            );
            // calculate market demands
            market.demand = {};
            entities.view<trader, site>().each (
                [&](auto& trader, auto& trader_site) {
                    if (in_market(trader_site, market_site, market)) {
                        for (auto [commodity, bid] : trader.bid) {
                            //TODO: make bids only in increments
                            market.demand[commodity] += std::floor(bid * (1.0 / 0.01)) / (1 / 0.01);
                        }
                    }
                }
            );
            // calculate market prices
            for (auto& [commodity, price] : market.prices) {
                const double base_price = entities.get<te::commodity>(commodity).base_price;
                const double demand = market.demand[commodity];
                const int stock = inventory.stock[commodity];
                const double disparity = static_cast<int>(demand) - stock;
                price = glm::clamp (
                    price + disparity * 0.0002,
                    base_price * 0.5,
                    base_price * 1.5
                );
            }
            // make trades
            entities.view<commodity>().each (
                [&](entt::entity commodity_e, auto& commodity) {
                    int curr_stock = inventory.stock[commodity_e];
                    double curr_demand = market.demand[commodity_e];
                    int demand_units = static_cast<int>(curr_demand);
                    double units_to_move = std::min(curr_stock, demand_units);
                    inventory.stock[commodity_e] -= units_to_move;
                    //TODO: decide how to pair up sellers
                    while (units_to_move > 0.0) {
                        entities.view<trader, site>().each (
                            [&](auto& trader, auto& trader_site) {
                                if (in_market(trader_site, market_site, market)) {
                                    double movement = std::min(units_to_move, trader.bid[commodity_e]);
                                    trader.bid[commodity_e] -= movement;
                                    units_to_move -= movement;
                                }
                            }
                        );
                    }
                    if (units_to_move != 0.0) {
                        spdlog::warn("movement residual of {}", units_to_move);
                    }
                }
            );
            // calculate market population
            int population = 0;
            entities.view<living, site>().each (
                [&](auto& trader, auto& dwelling_site) {
                    if (in_market(dwelling_site, market_site, market)) {
                        population++;
                    }
                }
            );
            // create/destroy dwellings
            entities.view<commodity>().each (
                [&](entt::entity commodity_e, auto& commodity) {
                    const double current_price = market.prices[commodity_e];
                    const double base_price = commodity.base_price;
                    const double markup = current_price / base_price;
                    const double demand = market.demand[commodity_e];
                    const int stock = inventory.stock[commodity_e];
                    const double disparity = static_cast<int>(demand) - stock;
                    if (disparity > (population * 2) && markup > 1.1) {
                        auto dwellings = entities.view<living, site>();
                        for (auto dwelling : dwellings) {
                            auto& dwelling_site = entities.get<site>(dwelling);
                            if (in_market(dwelling_site, market_site, market)) {
                                entities.destroy(dwelling);
                                break;
                            }
                        }
                    } else if (disparity < (population * 2) && markup < 0.9) {
                        spawn_dwelling(market_e);
                    }
                }
            );
        }
    );
}
