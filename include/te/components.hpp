#ifndef TE_COMPONENTS_HPP_INCLUDED
#define TE_COMPONENTS_HPP_INCLUDED

namespace te {
    // Render components
    struct render_tex {
        std::string filename;
    };
    template<typename Ar>
    void serialize(Ar& ar, render_tex& x){
        ar(x.filename);
    }

    struct render_mesh {
        std::string filename;
    };
    template<typename Ar>
    void serialize(Ar& ar, render_mesh& x){
        ar(x.filename);
    }

    struct noisy {
        std::string filename;
    };
    template<typename Ar>
    void serialize(Ar& ar, noisy& x){
        ar(x.filename);
    }

    struct pickable {
        int dummy = 0;
    };
    template<typename Ar>
    void serialize(Ar& ar, pickable& x){
        ar(x.dummy);
    }

    struct ghost {
        entt::entity proto;
    };
}

#endif
