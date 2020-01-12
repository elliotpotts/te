#include <te/sim.hpp>
#include <spdlog/spdlog.h>

te::sim::sim(unsigned int seed) : rengine { seed } {
    init_blueprints();
    generate_map();
}

void te::sim::init_blueprints() {
    families.resize(3);
    // Commodities
    auto wheat = commodities.emplace_back(entities.create());
    entities.assign<named>(wheat, "Wheat");
    entities.assign<price>(wheat, 15.0);
    entities.assign<render_tex>(wheat, "media/wheat.png");

    auto barley = commodities.emplace_back(entities.create());
    entities.assign<named>(barley, "Barley");
    entities.assign<price>(barley, 10.0);
    entities.assign<render_tex>(barley, "media/wheat.png");

    auto flour = commodities.emplace_back(entities.create());
    entities.assign<named>(flour, "Flour");
    entities.assign<price>(flour, 30.0);
    entities.assign<render_tex>(flour, "media/flour.png");

    std::unordered_map<entt::entity, double> base_market_prices;
    for (auto e : commodities) {
        base_market_prices[e] = entities.get<price>(e).price;
    }

    // Buildings
    auto wheat_field = blueprints.emplace_back(entities.create());
    entities.assign<named>(wheat_field, "Wheat Field");
    entities.assign<footprint>(wheat_field, glm::vec2{2.0f,2.0f});
    entities.assign<generator>(wheat_field, wheat, 1.0 / 4.0);
    entities.assign<inventory>(wheat_field);
    entities.assign<trader>(wheat_field, 0u);
    entities.assign<render_mesh>(wheat_field, "media/wheat.glb");
    entities.assign<pickable>(wheat_field);

    auto barley_field = blueprints.emplace_back(entities.create());
    entities.assign<named>(barley_field, "Barley Field");
    entities.assign<footprint>(barley_field, glm::vec2{2.0f,2.0f});
    entities.assign<generator>(barley_field, barley, 1.0 / 3.0);
    entities.assign<inventory>(barley_field);
    entities.assign<trader>(barley_field, 0u);
    entities.assign<render_mesh>(barley_field, "media/barley.glb");
    entities.assign<pickable>(barley_field);

    auto dwelling = blueprints.emplace_back(entities.create());
    entities.assign<named>(dwelling, "Dwelling");
    entities.assign<footprint>(dwelling, glm::vec2{1.0f,1.0f});
    demander& dwelling_demander = entities.assign<demander>(dwelling);
    dwelling_demander.rate[wheat] = 0.0005f;
    dwelling_demander.rate[barley] = 0.0004f;
    entities.assign<dweller>(dwelling);
    entities.assign<render_mesh>(dwelling, "media/dwelling.glb");
    entities.assign<pickable>(dwelling);
    
    auto market = blueprints.emplace_back(entities.create());
    entities.assign<named>(market, "Market");
    entities.assign<price>(market, 900.0);
    entities.assign<footprint>(market, glm::vec2{2.0f,2.0f});
    entities.assign<te::market>(market, base_market_prices);
    entities.assign<render_mesh>(market, "media/market.glb");
    entities.assign<pickable>(market);

    auto mill = blueprints.emplace_back(entities.create());
    entities.assign<named>(mill, "Flour Mill");
    entities.assign<footprint>(mill, glm::vec2{1.0f, 1.0f});
    std::unordered_map<entt::entity, double> inputs;
    inputs[wheat] = 4.0;
    std::unordered_map<entt::entity, double> outputs;
    outputs[flour] = 1.0;
    entities.assign<inventory>(mill);
    entities.assign<producer>(mill, inputs, outputs, 1.0 / 6.0);
    entities.assign<trader>(mill, 0u);
    entities.assign<price>(mill, 550.0);
    entities.assign<render_mesh>(mill, "media/mill.glb");
    entities.assign<pickable>(mill);
}

void te::sim::generate_map() {
    std::discrete_distribution<std::size_t> select_blueprint {7, 7, 2, 1, 2};
    for (int i = 0; i < 100; i++) spawn(blueprints[select_blueprint(rengine)]);
    // create a roaming merchant
    auto merchant_e = entities.create();
    entities.assign<named>(merchant_e, "Nebuchadnezzar");
    entities.assign<site>(merchant_e, glm::vec2{0.0f, 0.0f});
    entities.assign<footprint>(merchant_e, glm::vec2{1.0f, 1.0f});
    entities.assign<render_mesh>(merchant_e, "media/merchant.glb");
    entities.assign<pickable>(merchant_e);
    entities.assign<trader>(merchant_e, 1u);
    entities.assign<inventory>(merchant_e);
    std::vector<te::stop> salesman;
    int i = 1;
    for (auto market : entities.view<site, market>()) {
        te::stop buy_one_wheat { market };
        buy_one_wheat.leave_with[commodities[0]] = i++ % 2;
        salesman.push_back(buy_one_wheat);
    }
    entities.assign<merchant> (
        merchant_e,
        route { salesman },
        salesman.size() - 1
    );
    // Create a reverse salesman
    std::reverse(salesman.begin(), salesman.end());
    auto merchant2_e = entities.create(merchant_e, entities);
    entities.replace<named>(merchant2_e, "Marco Polo");
    entities.get<merchant&>(merchant2_e).route.stops = salesman;
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
bool te::sim::can_place(entt::entity entity, glm::vec2 centre) {
    {
        auto& print = entities.get<footprint>(entity);
        glm::vec2 topleft = centre - print.dimensions / 2.0f;
        for (int x = 0; x < print.dimensions.x; x++) {
            for (int y = 0; y < print.dimensions.y; y++) {
                if (
                    grid.find({topleft.x + x, topleft.y + y}) != grid.end()
                    || centre.x + x >  map_width / 2
                    || centre.x + x < -map_width / 2
                    || centre.y + y >  map_height / 2
                    || centre.y + y < -map_height / 2
                ) {
                    return false;
                }
            }
        }
    }
    if (auto maybe_market = entities.try_get<market>(entity); maybe_market) {
        auto other_markets = entities.view<site, market>();
        const bool conflict = std::any_of (
            other_markets.begin(),
            other_markets.end(),
            [&] (auto other) {
                const auto& [other_site, other_market] = other_markets.get<site, market>(other);
                return glm::length(glm::vec2{centre - other_site.position})
                    <= (maybe_market->radius + other_market.radius);
            }
        );
        if (conflict) return false;
    }
    return true;
}

std::optional<entt::entity> te::sim::try_place(entt::entity proto, glm::vec2 centre) {
    if (!can_place(proto, centre)) {
        return {};
    }
    
    auto instantiated = entities.create(proto, entities);
    entities.assign<site>(instantiated, centre);
    entities.replace<named>(instantiated, fmt::format("{} (#{})", entities.get<named>(instantiated).name, static_cast<unsigned>(instantiated)));
    
    //TODO: remove this horrible junk
    if (auto maybe_market = entities.try_get<te::market>(instantiated); maybe_market) {
        auto commons = entities.create();
        entities.assign<trader>(commons, 0u);
        entities.assign<inventory>(commons);
        entities.assign<site>(commons, centre);
        maybe_market->commons = commons;
    }
    
    auto& print = entities.get<footprint>(instantiated);
    glm::vec2 topleft = centre - print.dimensions / 2.0f;
    for (int x = 0; x < print.dimensions.x; x++) {
        for (int y = 0; y < print.dimensions.y; y++) {
            grid[{topleft.x + x, topleft.y + y}] = instantiated;
        }
    }
    return instantiated;
}

glm::vec2 te::sim::snap(glm::vec2 pos, glm::vec2 print) const {
    return round(pos - print / 2.0f) + print / 2.0f;
}

void te::sim::spawn(entt::entity proto) {
    const auto print = entities.get<footprint>(proto);
    const double min_x = -map_width / 2.0;
    std::uniform_real_distribution select_x_pos {min_x, min_x + map_width};
    const double min_y = -map_height / 2.0;
    std::uniform_real_distribution select_y_pos {min_y, min_y + map_height};
try_again:
    const glm::vec2 topleft {
        std::round(select_x_pos(rengine)),
        std::round(select_y_pos(rengine))
    };
    const glm::vec2 centre = topleft + print.dimensions / 2.0f;
    if (!try_place(proto, centre))
        goto try_again;
}

bool te::sim::spawn_dwelling(entt::entity market_e) {
    const auto& [market_site, market] = entities.get<site, te::market>(market_e);
    std::uniform_real_distribution select_x_pos {
        market_site.position.x - market.radius,
        market_site.position.x + market.radius
    };
    std::uniform_real_distribution select_y_pos {
        market_site.position.y - market.radius,
        market_site.position.y + market.radius
    };
    //TODO: un-hardcode this
    auto dwelling_blueprint = blueprints[2];
    int attempts = 0;
try_again:
    const glm::vec2 topleft {
        std::round(select_x_pos(rengine)),
        std::round(select_y_pos(rengine))
    };
    const glm::vec2 centre = topleft + glm::vec2{1.0f, 1.0f} / 2.0f;
    if (!try_place(dwelling_blueprint, centre)) {
        if (attempts++ >= 5) {
            return false;
        } else {
            goto try_again;
        }
    } else {
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

void te::sim::tick(double dt) {
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
                    } else {
                        // do nothing - wait for trades to finish
                    }
                } else {
                    merchant.trading = true;
                    auto& bids = entities.get<trader>(merchant_e).bid;
                    
                    for (auto [commodity_e, count] : dest_stop.leave_with) {
                        bids[commodity_e] = count - merchant_inventory.stock[commodity_e];
                    }
                }
            } else {
                // move him a bit closer
                merchant_site.position += glm::normalize(course) * static_cast<float>(dt);
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
                            generator.progress += generator.rate * dt;
                        } else if (generator.progress >= 1.0 && inventory.stock[generator.output] < 10) {
                            inventory.stock[generator.output]++;
                            trader.bid[generator.output] -= 1.0;
                            generator.progress -= 1.0;
                        }
                    }
                }
            );

            // advance producers
            entities.view<producer, inventory, site, trader>().each (
                [&](auto& producer, auto& inventory, auto& site, auto& trader) {
                    if (in_market(site, market_site, market)) {
                        if (producer.producing) {
                            producer.progress += producer.rate * dt;
                            if (producer.progress > 1.0) {
                                for (auto [commodity, produced] : producer.outputs) {
                                    inventory.stock[commodity] += produced;
                                    trader.bid[commodity] -= produced;
                                }
                                producer.progress = 0.0;
                                producer.producing = false;
                            }
                        } else {
                            bool enough = std::all_of (
                                producer.inputs.begin(),
                                producer.inputs.end(),
                                [&](auto pair) {
                                    auto [commodity, needed] = pair;
                                    return inventory.stock[commodity] >= needed;
                                }
                            );
                            if (enough) {
                                for (auto [commodity, needed] : producer.inputs) {
                                    inventory.stock[commodity] -= needed;
                                }
                                producer.producing = true;
                            } else {
                                for (auto [commodity, needed] : producer.inputs) {
                                    trader.bid[commodity] = std::max(0.0, needed - inventory.stock[commodity]);
                                }
                            }
                        }
                    }
                }
            );
           
            // demanders cause the market trader to demand more
            entities.view<demander, site>().each (
                [&](auto& demander, auto& demander_site) {
                    if (in_market(demander_site, market_site, market)) {
                        for (auto [commodity_e, demand_rate] : demander.rate) {
                            entities.get<trader>(market.commons).bid[commodity_e] += demand_rate * dt;
                        }
                    }
                }
            );

            for (auto commodity_e : commodities) {
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
                                                a_bid -= movement;
                                                a_stock += movement;
                                                trader_a.balance -= price;
                                                families[trader_a.family_ix].balance -= price;
                                                b_bid += movement;
                                                b_stock -= movement;
                                                trader_b.balance += price;
                                                families[trader_b.family_ix].balance += price;
                                            }
                                        }
                                    }
                                }
                            }
                        );
                    }
                );
            }
            
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
                const double base_price = entities.get<te::price>(commodity).price;
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
            auto dwellings = entities.view<dweller, site>();
            for (auto entity : dwellings) {
                auto& dwelling_site = dwellings.get<te::site>(entity);
                if (in_market(dwelling_site, market_site, market)) {
                    market.population++;
                }
            };

            // calculate market growth rate
            market.growth_rate = 0.0;
            for (auto commodity : commodities) {
                auto base_price = entities.get<price>(commodity).price;
                market.growth_rate += ((base_price - market.prices[commodity]) / base_price) * 0.1;
            }
            market.growth_rate = glm::clamp(market.growth_rate, -1.0, 1.0);

            // grow
            market.growth += market.growth_rate * dt;

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
