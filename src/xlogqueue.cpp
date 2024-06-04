#include <chrono>
#include <mutex>
#include <optional>
#include <queue>
#include <future>
#include "config.hpp"
#include "debug.hpp"
#include "xlog.hpp"

namespace xlog {
    namespace queue {
        static std::queue<xlog::queue::log_entry_t> g_queue{};
        static std::mutex g_queue_lock{};
        static std::optional<std::future<void>> g_worker_handle{};

        void insert(const xlog::queue::log_entry_t& data) {
            xlog::queue::g_queue_lock.lock();

            if (g_queue.size() >= config::field_maximum_log_entries) {
                debug::print("log-queue", "the limit of {} log entries has been reached, popping oldest log", config::field_maximum_log_entries);
                xlog::queue::g_queue.pop();
            }

            xlog::queue::g_queue.push(data);
            xlog::queue::g_queue_lock.unlock();
        }

        static void worker() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config::field_dispatch_sleep_ms));
                std::lock_guard<std::mutex> scope_lock(xlog::queue::g_queue_lock);

                while (!xlog::queue::g_queue.empty()) {
                    xlog::queue::log_entry_t entry{xlog::queue::g_queue.front()};
                    xlog::queue::g_queue.pop();

                    if (config::field_verbose) {
                        debug::print("log-queue", "dispatching: identififer: '{}', timestamp: '{}', message: '{}'", std::get<0>(entry), std::get<1>(entry), std::get<2>(entry));
                    }

                    // TODO: try to send the entries in a batch
                }

                (void)(scope_lock);
            }
        }

        bool start() {
            xlog::queue::g_worker_handle = std::make_optional(std::async(xlog::queue::worker));
            debug::print("log-journal", "queue started");

            return true;
        }
    }
}
