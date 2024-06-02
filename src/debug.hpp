#ifndef __DEBUG_HPP
#define __DEBUG_HPP

#include <format>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string_view>

namespace debug {
    extern std::mutex g_stream_lock;
    extern std::ofstream g_stream;

    template<typename ...Args>
    static void print(const char* module, const char* format, Args... args) {
        const std::lock_guard<std::mutex> _lock(debug::g_stream_lock);

        if (debug::g_stream.is_open()) {
            std::string message{std::vformat(format, std::make_format_args(args...))};

            if (!message.empty()) {
                auto hires_clock{std::chrono::high_resolution_clock::now()};
                auto clock{std::chrono::system_clock::now()};

                auto time_now_ms{std::chrono::time_point_cast<std::chrono::milliseconds>(hires_clock)};
                auto second_ms{time_now_ms.time_since_epoch().count() % 1000};

                std::time_t time_now{std::chrono::system_clock::to_time_t(clock)};
                
                #ifdef _WIN32
                    std::tm tm_now{};
                    localtime_s(&tm_now, &time_now);
                #else
                    std::tm tm_now{*localtime(&time_now)};
                #endif

                std::stringstream time_friendly{};
                time_friendly << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
                std::string debug_message{std::format("[{}.{:03}][{}] {}\n", time_friendly.view(), second_ms, module, message)};

                if (!debug_message.empty()) {
                    debug::g_stream.write(debug_message.data(), debug_message.length());
                    std::cout << debug_message;
                }
            }
        }
    }

    bool initialize();
}

#endif
