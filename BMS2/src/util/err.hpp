#ifndef UTIL_ERR_HPP
#define UTIL_ERR_HPP

#include <stdio.h>
#include <stdlib.h>

namespace util {
    // Macro which can be used to check a condition,
    // and terminate the program in case the condition is false.
    #define UTIL_CHECK_REQUIRE(x) do {                                                          \
        bool _cond = (x);                                                                       \
        if (unlikely(!(_cond))) {                                                               \
            printf("UTIL_CHECK_REQUIRE failed: %s at %s:%d\n", #x, __FILE__, __LINE__); \
            abort();                                                                            \
        }                                                                                       \
    } while(0)
} // namespace util


#endif // UTIL_ERR_HPP