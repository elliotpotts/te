#include <te/sim.hpp>
#include <te/csv_parser.hpp>
#include <te/app.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <utility>

te::sim::sim(unsigned int seed) : rengine { seed } {
    load_commodities();
    init_blueprints();
}

void te::sim::generate_map() {
    std::discrete_distribution<std::size_t> select_blueprint {7, 7, 2, 0, 2};
    for (int i = 0; i < 40; i++) spawn(blueprints[select_blueprint(rengine)]);
}

entt::entity te::sim::make_net_entity(unsigned family) {
    static int counter = 2000 + 2000 * family;
    auto created = entities.create(entt::entity { ++counter });
    new_entities.push_back(created);
    return created;
}

void te::sim::load_commodities() {
    static const std::vector<std::string> only_see {
        "Barley", "Carnelian", "Flax", "Furniture", "Linen cloth", "Wood"
    };
    std::ifstream fstr("assets/commodities.csv");
    std::string line;
    // skip header
    std::getline(fstr, line);
    while (std::getline(fstr, line)) {
        csv_parser csv {line.cbegin(), line.cend()};
        auto name = csv.parse_quoted();
        auto find_name = std::find(only_see.begin(), only_see.end(), name);
        if (find_name != only_see.end()) {
            auto entity = commodities.emplace_back(entities.create());
            entities.emplace<named>(entity, name);
            entities.emplace<price>(entity, csv.parse_double());
            entities.emplace<render_tex>(entity, fmt::format("assets/commodities/icons/{}.png", name));
        }
    }
}

void te::sim::init_blueprints() {
    families.resize(3);
    // Commodities
    std::unordered_map<entt::entity, double> base_market_prices;
    for (auto e : commodities) {
        base_market_prices[e] = entities.get<price>(e).price;
    }

    // Buildings
    auto barley_field = blueprints.emplace_back(entities.create());
    entities.emplace<named>(barley_field, "Barley Field");
    entities.emplace<described>(barley_field, "Barley fields produce a commodity demanded by dwellings. Supplying barley will allow the population at a market to increase.");
    entities.emplace<footprint>(barley_field, glm::vec2{2.0f,2.0f});
    entities.emplace<generator>(barley_field, false, commodities[0], 1.0 / 14.0);
    entities.emplace<inventory>(barley_field);
    entities.emplace<trader>(barley_field, 0u);
    entities.emplace<render_mesh>(barley_field, "assets/barley.glb");
    entities.emplace<pickable>(barley_field);

    auto flax_field = blueprints.emplace_back(entities.create());
    entities.emplace<named>(flax_field, "Flax Field");
    entities.emplace<described>(flax_field, "Flax fields produce a commodity demanded by dwellings. Supplying barley will allow the population at a market to increase.");
    entities.emplace<footprint>(flax_field, glm::vec2{2.0f,2.0f});
    entities.emplace<generator>(flax_field, false, commodities[2], 1.0 / 10.0);
    entities.emplace<inventory>(flax_field);
    entities.emplace<trader>(flax_field, 0u);
    entities.emplace<render_mesh>(flax_field, "assets/wheat.glb");
    entities.emplace<pickable>(flax_field);

    auto dwelling = blueprints.emplace_back(entities.create());
    entities.emplace<named>(dwelling, "Dwelling");
    entities.emplace<footprint>(dwelling, glm::vec2{1.0f,1.0f});
    demander& dwelling_demander = entities.emplace<demander>(dwelling);
    dwelling_demander.rate[commodities[0]] = 1.0 / (2*60.0 + 45.0); // it takes 2m45s to demand 1x wheat
    dwelling_demander.rate[commodities[3]] = 1.0 / (9*60.0);
    dwelling_demander.rate[commodities[4]] = 1.0 / (10*60.0);
    entities.emplace<dweller>(dwelling);
    entities.emplace<render_mesh>(dwelling, "assets/dwelling.glb");
    entities.emplace<pickable>(dwelling);

    auto market = blueprints.emplace_back(entities.create());
    entities.emplace<named>(market, "Trading Post");
    entities.emplace<price>(market, 200.0);
    entities.emplace<footprint>(market, glm::vec2{2.0f,2.0f});
    entities.emplace<te::market>(market, base_market_prices);
    entities.emplace<render_mesh>(market, "assets/market.glb");
    entities.emplace<noisy>(market, "assets/sfx/market2.wav");
    entities.emplace<pickable>(market);

    auto weaver = blueprints.emplace_back(entities.create());
    entities.emplace<named>(weaver, "Weaver");
    entities.emplace<footprint>(weaver, glm::vec2{1.0f, 1.0f});
    std::unordered_map<entt::entity, double> inputs;
    inputs[commodities[2]] = 3.0;
    std::unordered_map<entt::entity, double> outputs;
    outputs[commodities[4]] = 1.0;
    entities.emplace<inventory>(weaver);
    entities.emplace<producer>(weaver, inputs, outputs, 1.0 / 12.0);
    entities.emplace<trader>(weaver, 0u);
    entities.emplace<price>(weaver, 1000.0);
    entities.emplace<render_mesh>(weaver, "assets/mill.glb");
    entities.emplace<pickable>(weaver);
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
        const auto& print = entities.get<footprint>(entity);
        const glm::vec2 topleft = centre - print.dimensions / 2.0f;
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

std::optional<entt::entity> te::sim::try_place(unsigned owner, entt::entity proto, glm::vec2 centre) {
    if (!can_place(proto, centre)) {
        return {};
    }

    auto instantiated = make_net_entity(owner);
    entities.emplace<site>(instantiated, centre);
    if (auto c = entities.try_get<named>(proto)) entities.emplace<named>(instantiated, fmt::format("{} (#{})", c->name, static_cast<unsigned>(instantiated)));
    if (auto c = entities.try_get<described>(proto)) entities.emplace<described>(instantiated, *c);
    if (auto c = entities.try_get<footprint>(proto)) entities.emplace<footprint>(instantiated, *c);
    if (auto c = entities.try_get<demander>(proto)) entities.emplace<demander>(instantiated, *c);
    if (auto c = entities.try_get<trader>(proto)) entities.emplace<trader>(instantiated, *c);
    if (auto c = entities.try_get<generator>(proto)) entities.emplace<generator>(instantiated, *c);
    if (auto c = entities.try_get<producer>(proto)) entities.emplace<producer>(instantiated, *c);
    if (auto c = entities.try_get<market>(proto)) entities.emplace<market>(instantiated, *c);
    if (auto c = entities.try_get<inventory>(proto)) entities.emplace<inventory>(instantiated, *c);
    //TODO: Move non-sim related stuff out
    if (auto c = entities.try_get<render_mesh>(proto)) entities.emplace<render_mesh>(instantiated, *c);
    if (auto c = entities.try_get<pickable>(proto)) entities.emplace<pickable>(instantiated, *c);
    if (auto c = entities.try_get<noisy>(proto)) entities.emplace<noisy>(instantiated, *c);

    auto& print = entities.get<footprint>(instantiated);
    glm::vec2 topleft = centre - print.dimensions / 2.0f;
    for (int x = 0; x < print.dimensions.x; x++) {
        for (int y = 0; y < print.dimensions.y; y++) {
            grid[{topleft.x + x, topleft.y + y}] = instantiated;
        }
    }

    //TODO: somehow get rid of this special casing
    if (auto market = entities.try_get<te::market>(instantiated); market) {
        auto commons = make_net_entity(owner);
        entities.emplace<trader>(commons, 0u);
        entities.emplace<named>(commons, fmt::format("Commons (#{})", static_cast<unsigned>(commons)));
        entities.emplace<inventory>(commons);
        market->commons = commons;
        market->trading.push_back(commons);
        // make things trade
        auto traders = entities.view<site, trader>();
        for (auto trader_e : traders) {
            if (!entities.try_get<te::merchant>(trader_e) && in_market(entities.get<site>(trader_e), {centre}, *market)) {
                market->trading.push_back(trader_e);
            }
        }
    } else if (auto trader = entities.try_get<te::trader>(instantiated); trader) {
        auto markets = entities.view<te::site, te::market>();
        for (auto market_e : markets) {
            auto& market = markets.get<te::market>(market_e);
            if (in_market({centre}, markets.get<site>(market_e), market)) {
                market.trading.push_back(instantiated);
                break;
            }
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
    if (!try_place(0, proto, centre))
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
    if (!try_place(0, dwelling_blueprint, centre)) {
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
    int tot = 0;
    auto& market = entities.get<te::market>(market_e);
    for (auto trader_e : market.trading) {
        tot -= std::min(0.0, entities.get<te::trader>(trader_e).bid[commodity_e]);
    }
    return tot;
}

void te::sim::merchant_embark(entt::entity merchant_e, const te::route& route) {
    entities.get<te::merchant>(merchant_e).route = route;
    auto& bid = entities.get<te::trader>(merchant_e).bid;
    bid.clear();
    for (auto [commodity, leave_with] : route.stops[0].leave_with) {
        bid[commodity] = static_cast<double>(leave_with);
    }
}

std::optional<te::merchant_activity> te::sim::merchant_status(entt::entity merchant_e) {
    auto& merchant = entities.get<te::merchant>(merchant_e);
    if (merchant.route) {
        stop dest = merchant.route->stops[merchant.next_stop_ix];
        auto& dest_market = entities.get<te::market>(dest.where);
        auto it = std::find(dest_market.trading.begin(), dest_market.trading.end(), merchant_e);
        if (it != dest_market.trading.end()) {
            return te::merchant_activity{true, dest};
        } else {
            return te::merchant_activity{false, dest};
        }
    } else {
        return std::nullopt;
    }
}

void te::sim::tick_merchants(double dt) {
    auto merchants = entities.view<merchant, inventory, trader>();
    for (auto merchant_e : merchants) {
        auto& merchant = merchants.get<te::merchant>(merchant_e);
        if (!merchant.route) continue;
        te::stop& dest_stop = merchant.route->stops[merchant.next_stop_ix];
        auto& dest_market = entities.get<te::market>(dest_stop.where);
        auto dest = entities.get<te::site>(dest_stop.where);
        auto market_it = std::find(dest_market.trading.begin(), dest_market.trading.end(), merchant_e);
        if (market_it != dest_market.trading.end()) {
            // inside the market
            auto& merchant_inventory = merchants.get<te::inventory>(merchant_e);
            auto& merchant_trader = merchants.get<te::trader>(merchant_e);
            bool stop_satisfied = std::all_of (
                commodities.begin(),
                commodities.end(),
                [&](entt::entity commodity) {
                    return merchant_inventory.stock[commodity] == dest_stop.leave_with[commodity];
                }
            );
            if (stop_satisfied) {
                dest_market.trading.erase(market_it);
                entities.emplace<te::site>(merchant_e, dest);
                merchant.next_stop_ix = (merchant.next_stop_ix + 1) % merchant.route->stops.size();
                auto& next_stop = merchant.route->stops[merchant.next_stop_ix];
                auto& bids = merchant_trader.bid;
                for (auto commodity_e : commodities) {
                    bids[commodity_e] = next_stop.leave_with[commodity_e] - merchant_inventory.stock[commodity_e];
                }
            } else {
                // wait until it's satisfied
            }
        } else {
            // en route
            auto& merchant_site = entities.get<te::site>(merchant_e);
            auto course = dest.position - merchant_site.position;
            auto distance = glm::length(course);
            if (distance <= 1.0) {
                entities.remove<te::site>(merchant_e);
                dest_market.trading.push_back(merchant_e);
            } else {
                merchant_site.position += glm::normalize(course) * static_cast<float>(dt);
            }
        }
    }
}

void te::sim::tick(double dt, bool quiet) {
    tick_merchants(dt);
    entities.view<market, site>().each (
        [&](entt::entity market_e, auto& market, auto& market_site) {
            // advance generators
            entities.view<generator, inventory, trader, site>().each (
                [&](auto& generator, auto& inventory, auto& trader, auto& generator_site) {
                    if (in_market(generator_site, market_site, market)) {
                        generator.active = true;
                        if (generator.progress < 1.0) {
                            generator.progress += generator.rate * dt;
                        } else if (generator.progress >= 1.0 && inventory.stock[generator.output] < 10) {
                            inventory.stock[generator.output]++;
                            trader.bid[generator.output] -= 1.0;
                            generator.progress -= 1.0;
                        }
                    } else {
                        generator.active = false;
                    }
                }
            );

            // advance producers
            for (auto e : entities.view<producer, inventory, site, trader>()) {
                auto& producer = entities.get<te::producer>(e);
                auto& inventory = entities.get<te::inventory>(e);
                auto& site = entities.get<te::site>(e);
                auto& trader = entities.get<te::trader>(e);
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

            // Do trades
            for (auto commodity_e : commodities) {
                // TODO: sort the traders somehow
                // TODO: make this not O(N^2)
                for (auto& trader_a_e : market.trading) {
                    auto& a_stock = entities.get<te::inventory>(trader_a_e).stock[commodity_e];
                    auto& a = entities.get<te::trader>(trader_a_e);
                    auto& a_bid = a.bid[commodity_e];
                    for (auto& trader_b_e : market.trading) {
                        if (trader_a_e == trader_b_e) continue;
                        auto& b_stock = entities.get<te::inventory>(trader_b_e).stock[commodity_e];
                        auto& b = entities.get<te::trader>(trader_b_e);
                        auto& b_bid = b.bid[commodity_e];
                        // TODO: extract function for doing trades
                        if (a_bid > 0.0 && b_bid < 0.0) {
                            auto movement = static_cast<int>(std::min(a_bid, std::abs(b_bid)));
                            if (movement != 0 && b_stock >= movement) {
                                on_trade();
                                auto price = market.prices[commodity_e];
                                a_bid -= movement;
                                a_stock += movement;
                                a.balance -= price;
                                families[a.family_ix].balance -= price;
                                b_bid += movement;
                                b_stock -= movement;
                                b.balance += price;
                                families[b.family_ix].balance += price;
                            }
                        }
                    }
                }
            }

            // market demand is sum of all trader demands
            market.demand = {};
            for (auto trader_e : market.trading) {
                const auto& trader = entities.get<te::trader>(trader_e);
                for (auto [commodity, bid] : trader.bid) {
                    market.demand[commodity] += std::max(0.0, std::floor(bid * (1.0 / 0.01)) / (1 / 0.01));
                }
            }

            /* Calculate market prices
             *   - prices should not increase unless there is at least 1 unit of demand */
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
            /*TODO: Need to get this figured out. The following should be taken into account:
             *  - the price of basic goods
             *  - how long dwellings have been without basic goods
             *  - ???
             */
            // for now, let's say markets grow when both wheat and barley are reasonably priced
            market.growth_rate = 0.0;
            for (auto commodity : commodities) {
                auto base_price = entities.get<price>(commodity).price;
                market.growth_rate += ((base_price - market.prices[commodity]) / base_price) * 0.1;
            }
            market.growth_rate = glm::clamp(market.growth_rate, -(1.0 / 3.0), 1.0 / 4.0);

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
