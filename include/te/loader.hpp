#ifndef TE_LOADER_HPP_INCLUDED
#define TE_LOADER_HPP_INCLUDED
#include <unordered_map>
#include <string>
#include <te/unique_any.hpp>
#include <te/gl.hpp>
#include <spdlog/spdlog.h>
namespace te {
    template<typename T>
    T load_from_file(te::gl::context& gl, std::string name);
    
    class loader {
        gl::context& gl;
        std::unordered_map<std::string, unique_any> loaded;
    public:
        loader(gl::context& gl) : gl(gl) {
        }
        
        template<typename T>
        T& lazy_load(const std::string& filename) {
            auto loaded_it = loaded.find(filename);
            if (loaded_it == loaded.end()) {
                spdlog::info("Loading {}", filename);
                auto [it, action] = loaded.emplace (
                    std::piecewise_construct,
                    std::make_tuple(filename),
                    std::forward_as_tuple (
                        std::in_place_type_t<T>{},
                        std::forward<T>(load_from_file<T>(gl, filename))
                    )
                );
                return it->second.template get<T>();
            } else {
                return loaded_it->second.template get<T>();
            }
        }
    };
}
#endif
