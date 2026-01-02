#ifndef UTIL_ERR_HPP
#define UTIL_ERR_HPP

#include "esp_rom_sys.h"

namespace util {
    // Macro which can be used to check a condition,
    // and terminate the program in case the condition is false.
    #define UTIL_CHECK_ERR(x) do {                                                              \
        bool _cond = (x);                                                                       \
        if (unlikely(!(_cond))) {                                                               \
            esp_rom_printf("UTIL_CHECK_ERR failed: %s at %s:%d\n", #x, __FILE__, __LINE__);     \
            abort();                                                                            \
        }                                                                                       \
    } while(0)
} // namespace util


#endif // UTIL_ERR_HPP