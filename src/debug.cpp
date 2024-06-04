#include <array>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <ios>
#include <iostream>
#include <fstream>
#include <mutex>

#include "debug.hpp"

namespace debug {
    static const char* g_stream_filename{"debug.log"};

    std::mutex g_stream_lock{};
    std::ofstream g_stream{};

    bool initialize() {
        // flawfinder: ignore
        debug::g_stream.open(debug::g_stream_filename, std::ios_base::app);

        if (!debug::g_stream.is_open()) {
            return false;
        }

        return true;
    }
}
