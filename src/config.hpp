#ifndef __CONFIG_HPP
#define __CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace config {
    extern bool        field_verbose;
    extern int64_t     field_dispatch_sleep_ms;
    extern size_t      field_maximum_log_entries;
    extern int64_t     field_seconds_between_connects;
    extern std::string field_remote_address;
    extern uint16_t    field_remote_port;
    extern std::string field_remote_certificate;
    extern std::string field_identity;
    extern std::string field_identity_password;

    bool initialize();
}

#endif
