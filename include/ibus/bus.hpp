#ifndef IBUS_BUS_HPP_INCLUDED
#define IBUS_BUS_HPP_INCLUDED

#include <ibus-1.0/ibus.h>

namespace ibus {
    class bus {
        IBusBus* hnd;
    public:
        bus();
        IBusInputContext* make_context();
    };
}

#endif
