#ifndef __LOG_HPP
#define __LOG_HPP

#include <cstdint>
#include <string>
#include <tuple>

namespace xlog {
    bool initialize();

    namespace journald {
        bool start(std::string identifier);
        bool platform_support();
    }

    namespace queue {
        using log_entry_t = std::tuple<std::string, int64_t, std::string>;

        bool start();
        void insert(const log_entry_t& data);
    }
}

#endif
