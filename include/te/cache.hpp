#ifndef TE_CACHE_HPP_INCLUDED
#define TE_CACHE_HPP_INCLUDED
#include <unordered_map>
#include <string>
#include <te/util.hpp>
#include <te/unique_any.hpp>
#include <spdlog/spdlog.h>
namespace te {
    template<typename T>
    class cached {
    };
    
    template<typename F>
    class cache {
        std::unordered_map<std::string, unique_any> loaded;
        F& loader;
    public:
        cache(F& loader) : loader(loader) {
        }

        template<typename T>
        T& load(const std::string& filename) {
            spdlog::info("Loading {}", filename);
            auto [it, emplaced] = loaded.emplace (
                std::piecewise_construct,
                std::make_tuple(filename),
                std::forward_as_tuple (
                    std::in_place_type_t<T>{},
                    std::forward<T>(loader.operator()(type_tag<T>{}, filename))
                )
            );
            return it->second.template get<T>();
        }

        template<typename T>
        T& res(const std::string& filename) {
            auto loaded_it = loaded.find(filename);
            if (loaded_it == loaded.end()) {
                throw std::runtime_error(fmt::format("Resource \"{}\" has not been loaded!"));
            } else {
                return loaded_it->second.template get<T>();
            }
        }
        
        template<typename T>
        T& lazy_load(const std::string& filename) {
            auto loaded_it = loaded.find(filename);
            if (loaded_it == loaded.end()) {
                return load<T>(filename);
            } else {
                return loaded_it->second.template get<T>();
            }
        }
    };
}
#endif
