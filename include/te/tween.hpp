#ifndef TE_TWEEN_HPP_INCLUDED
#define TE_TWEEN_HPP_INCLUDED

namespace te {
    template<typename T>
    struct tween {
        T start;
        T end;
        const int period;
        int ticks;

        tween(T initial, int period) : start{initial}, end{initial}, period{period}, ticks{0} {
        }

        void to(T to) {
            start = end;
            end = to;
            if (start != end) {
                ticks = 0;
            }
        }

        void delta(T d) {
            to(end + d);
        }

        void factor(T f) {
            to(end * f);
        }

        T operator()() {
            double t = static_cast<double>(ticks) / static_cast<double>(period);
            double eased = 1.0 - std::pow(1.0 - t, 4.0);
            ticks++;
            return start + (end - start) * eased;
        }

        operator bool() {
            return ticks != period;
        }
    };
}

#endif
