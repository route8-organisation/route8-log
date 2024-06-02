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
    static const size_t g_buffer_size{13 * 1024};
    static const size_t g_message_buffer_size{12 * 1024};

    static std::mutex g_stream_lock{};
    static std::ofstream g_stream{};
    static std::array<char, debug::g_message_buffer_size> g_message_buffer{};
    static std::array<char, debug::g_buffer_size> g_buffer{};

    template<typename T, std::size_t N>
    static void log_message(const char* module, std::array<T, N>& message) {
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

        std::array<char, 128> time_friendly{};
        std::strftime(time_friendly.data(), time_friendly.size(), "%Y-%m-%d %H:%M:%S", &tm_now);

        auto nb_buffer{std::snprintf(debug::g_buffer.data(), debug::g_buffer.size(), "[%s.%.03ld][%s] %s\n", time_friendly.data(), static_cast<long>(second_ms), module, message.data())};

        if (nb_buffer) {
            debug::g_stream.write(debug::g_buffer.data(), nb_buffer);
            std::cout << g_buffer.data();
        }
    }

    void print(const char* module, const char* format, ...) {
        std::va_list va_args{};
        va_start(va_args, format);
        debug::g_stream_lock.lock();

        if (debug::g_stream.is_open()) {
            // flawfinder: ignore
            auto nb_message{std::vsnprintf(debug::g_message_buffer.data(), debug::g_message_buffer.size(), format, va_args)};

            if (nb_message) {
                debug::log_message(module, debug::g_message_buffer);
            }
        }

        debug::g_stream_lock.unlock();
        va_end(va_args);
    }

    bool initialize() {
        // flawfinder: ignore
        debug::g_stream.open(debug::g_stream_filename, std::ios_base::app);

        if (!debug::g_stream.is_open()) {
            return false;
        }

        return true;
    }
}
