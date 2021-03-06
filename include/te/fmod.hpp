#ifndef TE_FMOD_HPP_INCLUDED
#define TE_FMOD_HPP_INCLUDED

#include <memory>
#include <stdexcept>
#include <fmod.hpp>

namespace te {
    struct fmod_system_deleter {
        void operator()(FMOD::System* p) const;
    };
    using fmod_system_hnd = std::unique_ptr<FMOD::System, fmod_system_deleter>;
    fmod_system_hnd make_fmod_system();

    struct fmod_sound_deleter {
        void operator()(FMOD::Sound* p) const;
    };
    using fmod_sound_hnd = std::unique_ptr<FMOD::Sound, fmod_sound_deleter>;
};

#endif
