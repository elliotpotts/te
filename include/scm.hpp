#ifndef SCM_HPP_INCLUDED
#define SCM_HPP

#include <guile/3.0/libguile.h>

namespace scm {
    namespace {
        template<typename F>
        SCM in_try_body(void* data) {
            auto thunk = reinterpret_cast<F*>(data);
            return (*thunk)();
        }

        template<typename F>
        SCM in_try_handler(void* data, SCM key, SCM args) {
            //throw std::runtime_error("scheme exception");
            spdlog::debug("SPOOF!");
            return SCM_BOOL_F;
        }

        template<typename F>
        SCM in_try_preunwind_handler(void* data, SCM key, SCM args) {
            return SCM_BOOL_F;
        }

        template<typename T>
        SCM dispatch(SCM userdata, SCM args) {
            auto evald = (*reinterpret_cast<T*>(scm_to_pointer(userdata)))(args);
            SCM evald_scm = scm_from_signed_integer(evald);
            return evald_scm;
        }
    }

    //TODO: class which can be casted from blah blah
    class value {
        SCM val;
    public:
    };

    // Execute the given c++ code, catching scheme exceptions thrown
    // NOTE: c++ exceptions cannot be caught in this environment!
    template <typename F>
    SCM in_try(F&& f) {
        return scm_c_catch (
            SCM_BOOL_T,
            &in_try_body<F>, reinterpret_cast<void*>(&f),
            &in_try_handler<F>, reinterpret_cast<void*>(&f),
            &in_try_preunwind_handler<F>, reinterpret_cast<void*>(&f)
        );
    }

    //TODO: function traits
    //TODO: return types
    template<typename F>
    void define(const char* name, F* fun) {
        std::string dispatcher_name = fmt::format("__dispatch_{}", name);
        SCM dispatcher = scm_c_define_gsubr(dispatcher_name.c_str(), 2, 0, 0, reinterpret_cast<void*>(&dispatch<F>));
        auto src = fmt::format (
            "(use-modules (system foreign)) (define {} (lambda args ({} (make-pointer {}) args)))",
            name, dispatcher_name, reinterpret_cast<uintptr_t>(fun)
        );
        scm_c_eval_string(src.c_str());
    }
}

#endif
