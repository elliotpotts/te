#ifndef TE_UNIQUE_ANY_HPP_INCLUDED
#define TE_UNIQUE_ANY_HPP_INCLUDED

#include <memory>

namespace te {
    class unique_any {
        const std::type_info& contained_type;
        const std::unique_ptr<void, std::function<void(void*)>> store;    
    public:
        template<typename T, typename... Args>
        unique_any(std::in_place_type_t<T>, Args&&... args) :
            contained_type { typeid(T) },
            store {new T(std::forward<Args>(args)...),
                   [](void* object) {
                       std::default_delete<T>{}(static_cast<T*>(object));
                   }} {
            }
    
        template<typename T>
        T& get() {
            if (typeid(T) == contained_type) {
                return *static_cast<T*>(store.get());
            } else {
                throw std::runtime_error("Bad cast!");
            }
        }

        template<typename T>
        const T& get() const {
            if (typeid(T) == contained_type) {
                return *static_cast<T*>(store.get());
            } else {
                throw std::runtime_error("Bad cast!");
            }
        }
    };
}

#endif
