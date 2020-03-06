#include <te/fmod.hpp>
#include <fmt/format.h>
#include <fmod_errors.h>

void te::fmod_system_deleter::operator()(FMOD::System* const p) const {
    p->release();
}

te::fmod_system_hnd te::make_fmod_system() {
    FMOD::System* created;
    if (FMOD_RESULT result = FMOD::System_Create(&created); result != FMOD_OK) {
        throw std::runtime_error(fmt::format("Couldn't create fmod due to error {}: {}", result, FMOD_ErrorString(result)));
    } else {
        return te::fmod_system_hnd{created};
    }
}
