#ifndef __INET_HPP
#define __INET_HPP

#include <string>

namespace inet {
    bool send_log(std::string log_identifier, int64_t log_timestamp, std::string log_data);
    bool connect();
}

#endif
