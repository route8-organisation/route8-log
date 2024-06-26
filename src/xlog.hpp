#ifndef __LOG_HPP
#define __LOG_HPP

#include <cstdint>
#include <string>
#include <tuple>

namespace xlog {
    bool initialize();

    namespace queue {
        using log_entry_t = std::tuple<std::string, int64_t, std::string>;

        bool start();
        void insert(const log_entry_t& data);
    }

    namespace journald {
        bool start(std::string identifier);
        bool platform_support();
    }

    namespace winevent {
        bool start(std::string identifier, std::string source_name);
        bool platform_support();
    }

    namespace file {
        bool start(std::string identifier, std::string source_filename);
    }
}

#endif
