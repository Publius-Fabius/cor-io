#ifndef CORIO_DEFER_H
#define CORIO_DEFER_H

namespace corio {

    template<typename F> struct defer_type {
        F function;
        ~defer_type() noexcept {
            function();
        }
    };

#define DEFER_CONCAT_INNER(x, y) x##y
#define DEFER_CONCAT(x, y) DEFER_CONCAT_INNER(x, y)
#define defer(CODE) \
    auto DEFER_CONCAT(finally_block_, __LINE__) = defer_type{ [&](){ CODE } };

}

#endif