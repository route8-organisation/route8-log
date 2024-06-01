#include <chrono>
#include <mutex>
#include <optional>
#include <queue>
#include "config.hpp"
#include "coroutine"
#include "coroutine.hpp"
#include "debug.hpp"
#include "xlog.hpp"

namespace xlog {
    namespace queue {
        static std::queue<xlog::queue::log_entry_t> g_queue{};
        static std::mutex g_queue_lock{};
        static std::optional<routine> g_worker_handle{};

        void insert(const xlog::queue::log_entry_t& data) {
            xlog::queue::g_queue_lock.lock();
            xlog::queue::g_queue.push(data);
            xlog::queue::g_queue_lock.unlock();
        }

        struct lock_queue_routine {
            bool await_ready() const {
                return false;
            }

            void await_suspend(std::coroutine_handle<> handle) const {
                std::thread([handle]() {
                    xlog::queue::g_queue_lock.lock();
                    handle.resume();
                }).detach();
            }

            void await_resume() const {}
        };

        static routine worker() {
            while (true) {
                co_await sleep_routine{std::chrono::milliseconds(config::field_dispatch_sleep_ms)};
                co_await lock_queue_routine{};

                while (!xlog::queue::g_queue.empty()) {
                    xlog::queue::log_entry_t entry{xlog::queue::g_queue.front()};
                    xlog::queue::g_queue.pop();

                    if (config::field_verbose) {
                        debug::print("log-queue", "dispatching: identififer: '%s', timestamp: '%lld', message: '%s'", std::get<0>(entry).c_str(), std::get<1>(entry), std::get<2>(entry).c_str());
                    }
                }

                xlog::queue::g_queue_lock.unlock();
            }

            co_return;
        }

        bool start() {
            xlog::queue::g_worker_handle = std::make_optional(xlog::queue::worker());
            debug::print("log-journal", "queue started");

            return true;
        }
    }
}
