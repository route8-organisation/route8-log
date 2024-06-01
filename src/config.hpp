#ifndef __CONFIG_HPP
#define __CONFIG_HPP

#include <cstdint>

namespace config {
    extern bool field_verbose;
    extern int64_t field_dispatch_sleep_ms;

    bool initialize();
}

#endif
