#ifndef __CONFIG_HPP
#define __CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace config {
    extern bool field_verbose;
    extern int64_t field_dispatch_sleep_ms;
    extern size_t field_maximum_log_entries;

    bool initialize();
}

#endif
