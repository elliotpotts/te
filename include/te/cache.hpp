#ifndef TE_CACHE_HPP_INCLUDED
#define TE_CACHE_HPP_INCLUDED
#include <unordered_map>
#include <string>
#include <type_traits>
#include <te/util.hpp>
#include <te/unique_any.hpp>
#include <te/util.hpp>
#include <te/mesh.hpp>
#include <utility>
#include <spdlog/spdlog.h>
namespace te {
    struct asset_loader {
        te::gl::context& gl;
        te::gl::texture2d operator()(type_tag<te::gl::texture2d>, const std::string& filename);
        te::gltf operator()(type_tag<te::gltf>, const std::string& filename);
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
                    loader.operator()(type_tag<T>{}, filename)
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
