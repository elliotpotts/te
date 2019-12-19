#include <te/sim.hpp>
#include <spdlog/spdlog.h>

te::sim::sim(unsigned int seed) : rengine { seed } {
    init_blueprints();
    generate_map();
}
entt::entity global_wheat;
void te::sim::init_blueprints() {
    // Commodities
    auto wheat = entities.create();
    entities.assign<named>(wheat, "Wheat");
    entities.assign<commodity>(wheat, 15.0f);
    entities.assign<render_tex>(wheat, "media/wheat.png");
    //todo:remove
    global_wheat = wheat;

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
    entities.assign<generator>(wheat_field, wheat, 0.001);
    entities.assign<inventory>(wheat_field);
    entities.assign<trader>(wheat_field);
    entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
    entities.assign<pickable>(wheat_field);

    auto barley_field = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(barley_field, "Barley Field");
    entities.assign<site_blueprint>(barley_field, glm::vec2{2.0f,2.0f});
    entities.assign<generator>(barley_field, barley, 0.002);
    entities.assign<inventory>(barley_field);
    entities.assign<trader>(barley_field);
    entities.assign<render_mesh>(barley_field, "media/barley.glb");
    entities.assign<pickable>(barley_field);

    auto dwelling = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(dwelling, "Dwelling");
    entities.assign<site_blueprint>(dwelling, glm::vec2{1.0f,1.0f});
    demander& dwelling_demander = entities.assign<demander>(dwelling);
    dwelling_demander.rate[wheat] = 0.0005f;
    dwelling_demander.rate[barley] = 0.0004f;
    entities.assign<dweller>(dwelling);
    entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
    entities.assign<pickable>(dwelling);
    
    auto market = site_blueprints.emplace_back(entities.create());
    entities.assign<named>(market, "Market");
    entities.assign<site_blueprint>(market, glm::vec2{2.0f,2.0f});
    entities.assign<te::market>(market, base_market_prices);
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
    entities.assign<trader>(merchant_e);
    entities.assign<inventory>(merchant_e);
    std::vector<te::stop> salesman;
    int i = 1;
    for (auto market : entities.view<site, market>()) {
        te::stop buy_one_wheat { market };
        buy_one_wheat.leave_with[global_wheat] = i++ % 2;
        salesman.push_back(buy_one_wheat);
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
    if (where.x < 0 || where.x > map_width || where.y < 0 || where.y > map_height) {
        return false;
    }
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
    
    //TODO: remove this horrible junk
    if (auto maybe_market = entities.try_get<te::market>(instantiated); maybe_market) {
        auto commons = entities.create();
        entities.assign<trader>(commons);
        entities.assign<inventory>(commons);
        entities.assign<site>(commons, where);
        maybe_market->commons = commons;
    }
    
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

bool te::sim::spawn_dwelling(entt::entity market_e) {
    const auto& [market_site, market] = entities.get<site, te::market>(market_e);
    std::uniform_int_distribution select_x_pos {
        static_cast<int>(market_site.position.x - market.radius),
        static_cast<int>(market_site.position.x + market.radius)
    };
    std::uniform_int_distribution select_y_pos {
        static_cast<int>(market_site.position.y - market.radius),
        static_cast<int>(market_site.position.y + market.radius)
    };
    //TODO: un-hardcode this
    auto dwelling_blueprint = site_blueprints[2];
    int attempts = 0;
try_again:
    glm::vec2 map_pos { select_x_pos(rengine), select_y_pos(rengine) };
    if (!can_place(dwelling_blueprint, map_pos)) {
        if (attempts++ >= 5) {
            return false;
        } else {
            goto try_again;
        }
    } else {
        place(dwelling_blueprint, map_pos);
        entities.get<te::market>(market_e).population++;
        return true;
    }
}

int te::sim::market_stock(entt::entity market_e, entt::entity commodity_e) {
    const auto& market_site = entities.get<te::site>(market_e);
    const auto& market = entities.get<te::market>(market_e);
    int tot = 0;
    entities.view<te::site, te::trader>().each (
        [&](auto& site, auto& trader) {
            if (in_market(site, market_site, market) && trader.bid[commodity_e] < 0) {
                tot -= trader.bid[commodity_e];
            }
        }
    );
    return tot;
}

void te::sim::tick() {
    entities.view<merchant, named, inventory, site>().each (
        [&](entt::entity merchant_e, auto& merchant, auto& name, auto& merchant_inventory, auto& merchant_site) {
            std::size_t dest_stop_ix = (merchant.last_stop + 1) % merchant.route.stops.size();
            stop& dest_stop = merchant.route.stops[dest_stop_ix];
            auto dest = entities.get<site>(dest_stop.where);
            auto course = dest.position - merchant_site.position;
            auto distance = glm::length(course);
            // is the merchant at his desired market?
            if (distance <= 1.0f) {
                if (merchant.trading) {
                    if (merchant_inventory.stock == dest_stop.leave_with) {
                        // stop trading
                        merchant.trading = false;
                        // start heading to next market
                        merchant.last_stop = dest_stop_ix;
                        spdlog::debug("{} is en route to {}", name.name, entities.get<named>(dest_stop.where).name);
                    } else {
                        // do nothing - wait for trades to finish
                    }
                } else {
                    spdlog::debug("{} is has begun trading:", name.name);
                    merchant.trading = true;
                    auto& bids = entities.get<trader>(merchant_e).bid;
                    
                    for (auto [commodity_e, count] : dest_stop.leave_with) {
                        spdlog::debug("    want to leave with {}, so:", count);
                        bids[commodity_e] = count - merchant_inventory.stock[commodity_e];
                        spdlog::debug("    bid {} for {}", bids[commodity_e], entities.get<named>(commodity_e).name);
                    }
                }
            } else {
                // move him a bit closer
                merchant_site.position += glm::normalize(course) * 0.05f;
            }
        }
    );
    entities.view<market, site>().each (
        [&](entt::entity market_e, auto& market, auto& market_site) {
            // advance generators
            entities.view<generator, inventory, trader, site>().each (
                [&](auto& generator, auto& inventory, auto& trader, auto& generator_site) {
                    if (in_market(generator_site, market_site, market)) {
                        if (generator.progress < 1.0) {
                            generator.progress += generator.rate;
                        } else if (generator.progress >= 1.0 && inventory.stock[generator.output] < 10) {
                            inventory.stock[generator.output]++;
                            trader.bid[generator.output] -= 1.0;
                            generator.progress -= 1.0;
                        }
                    }
                }
            );
           
            // demanders cause the market trader to demand more
            entities.view<demander, site>().each (
                [&](auto& demander, auto& demander_site) {
                    if (in_market(demander_site, market_site, market)) {
                        for (auto [commodity_e, demand_rate] : demander.rate) {
                            entities.get<trader>(market.commons).bid[commodity_e] += demand_rate;
                        }
                    }
                }
            );

            entities.view<commodity>().each (
                [&](entt::entity commodity_e, auto&) {
                    //TODO: sort traders here
                    //TODO: make this not O(N^2)
                    //TODO: somehow deal with dwellings...
                    entities.view<inventory, site, trader>().each (
                        [&](entt::entity trader_a_e, auto& trader_a_inventory, auto& trader_a_site, auto& trader_a) {
                            entities.view<inventory, site, trader>().each (
                                [&](entt::entity trader_b_e, auto& trader_b_inventory, auto& trader_b_site, auto& trader_b) {
                                    if (in_market(trader_a_site, market_site, market) && in_market(trader_b_site, market_site, market)) {
                                        auto& a_bid = trader_a.bid[commodity_e];
                                        auto& b_bid = trader_b.bid[commodity_e];
                                        if (a_bid > 0.0 && b_bid < 0.0) {
                                            auto movement = static_cast<int>(std::min(a_bid, std::abs(b_bid)));
                                            auto& a_stock = trader_a_inventory.stock[commodity_e];
                                            auto& b_stock = trader_b_inventory.stock[commodity_e];
                                            if (movement != 0) {
                                                if (b_stock >= movement) {
                                                    auto price = market.prices[commodity_e];
                                                    spdlog::debug("movement {}x{} @{}", movement, entities.get<named>(commodity_e).name, price);
                                                    a_bid -= movement;
                                                    a_stock += movement;
                                                    trader_a.balance -= price;
                                                    b_bid += movement;
                                                    b_stock -= movement;
                                                    trader_b.balance += price;
                                                }
                                            }
                                        }
                                    }
                                }
                            );
                        }
                    );
                }
            );
            
            // market demand is sum of all trader demands
            market.demand = {};
            entities.view<trader, site>().each (
                [&](auto& trader, auto& trader_site) {
                    if (in_market(trader_site, market_site, market)) {
                        for (auto [commodity, bid] : trader.bid) {
                            //TODO: make bids only in increments
                            market.demand[commodity] += std::max(0.0, std::floor(bid * (1.0 / 0.01)) / (1 / 0.01));
                        }
                    }
                }
            );
           
            // calculate market prices
            for (auto& [commodity, price] : market.prices) {
                const double base_price = entities.get<te::commodity>(commodity).base_price;
                const double demand = market.demand[commodity];
                const int stock = market_stock(market_e, commodity);
                const double disparity = static_cast<int>(demand) - stock;
                price = glm::clamp (
                    price + disparity * 0.0002,
                    base_price * 0.5,
                    base_price * 1.5
                );
            }
            
            // calculate market population
            market.population = 0;
            entities.view<dweller, site>().each (
                [&](auto& trader, auto& dwelling_site) {
                    if (in_market(dwelling_site, market_site, market)) {
                        market.population++;
                    }
                }
            );

            // calculate market growth rate
            market.growth_rate = 0.0;
            entities.view<commodity>().each (
                [&] (auto& commodity_e, auto& commodity) {
                    market.growth_rate += ((commodity.base_price - market.prices[commodity_e]) / commodity.base_price) * 0.01;
                }
            );
            market.growth_rate = glm::clamp(market.growth_rate, -0.01, 0.01);

            // grow
            market.growth += market.growth_rate;

            // create/destroy dwellings
            while (static_cast<int>(market.growth) > 0 && spawn_dwelling(market_e)) {
                market.growth -= 1.0;
            }
            while (static_cast<int>(market.growth) < 0) {
                market.growth += 1.0;
                for (auto dwelling : entities.view<dweller, site>()) {
                    auto& dwelling_site = entities.get<site>(dwelling);
                    if (in_market(dwelling_site, market_site, market)) {
                        entities.destroy(dwelling);
                        break;
                    }
                }
            }
        }
    );
}
