#ifndef TE_UNIQUE_HPP_INCLUDED
#define TE_UNIQUE_HPP_INCLUDED
#include <optional>
namespace te {
    // Represent a unique opaque handle value
    // T = identity value representing object to which we only want one "reference" to
    template<typename T, typename Deleter>
    struct unique {
        std::optional<T> storage;
        unique(unique&& other) : storage(std::move(other.storage)) {
            other.storage.reset();
        }
        explicit unique(T hnd) : storage(hnd) {
        }
        unique(const unique&) = delete;
        const T& operator*() const {
            return *storage;
        }
        ~unique() {
            if (storage) {
                Deleter {}(storage.value());
            }
        }
    };
}
#endif
