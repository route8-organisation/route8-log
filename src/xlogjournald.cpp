#include <chrono>
#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <limits>

#ifndef _WIN32 
#include <systemd/sd-journal.h>
#endif

#include <tuple>
#include <utility>
#include <vector>
#include "nlohmann/json_fwd.hpp"
#include "nlohmann/json.hpp"
#include "config.hpp"
#include "coroutine.hpp"
#include "debug.hpp"
#include "xlog.hpp"

namespace xlog {
    namespace journald {
    #ifndef _WIN32 
        static sd_journal * g_journal_handle{nullptr};
        static std::optional<routine> g_worker_routine{};

        struct journal_wait_routine {
            sd_journal * journal_handle{nullptr};
            int mutable result_value{0};

            journal_wait_routine(sd_journal* value) {
                if (!value) {
                    throw std::runtime_error("journal_handle is null");
                }

                this->journal_handle = value;
            }

            bool await_ready() const {
                return false;
            }

            void await_suspend(std::coroutine_handle<> handle) const {
                std::thread([this, handle]() {
                    this->result_value=sd_journal_wait(journal_handle, std::numeric_limits<int64_t>::max());
                    handle.resume();
                }).detach();
            }

            int await_resume() const {
                return this->result_value;
            }
        };

        static bool journal_entry_procedure(sd_journal* journal, std::pair<int64_t, std::string>& result) {
            size_t data_nb{0};
            const void * data_c{nullptr};
            nlohmann::json entry_json{};
            auto timestamp = static_cast<int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

            SD_JOURNAL_FOREACH_DATA(journal, data_c, data_nb) {
                std::string data(const_cast<char*>(reinterpret_cast<const char*>(data_c)), data_nb);
                auto split_idx{data.find_first_of('=')};

                if (split_idx == std::string::npos) {
                    debug::print("log-journald", "failed to split key value entry, error: missing '=' on '%s'", data.c_str());
                    continue;
                }

                std::string key(data.begin(), data.begin() + split_idx);
                std::string value(data.begin() + split_idx + 1, data.end());
                entry_json[key] = value;
            }

            result = std::make_pair(timestamp, entry_json.dump());

            return true;
        }

        static routine worker(std::string identifier) {
            while (true) {
                char* initial_cursor{nullptr};
                sd_journal_seek_tail(xlog::journald::g_journal_handle);

                if (sd_journal_previous(xlog::journald::g_journal_handle)) {
                    sd_journal_get_cursor(xlog::journald::g_journal_handle, &initial_cursor);
                }

                int wait_result{co_await xlog::journald::journal_wait_routine{xlog::journald::g_journal_handle}};

                if (wait_result == SD_JOURNAL_APPEND) {
                    std::vector<xlog::queue::log_entry_t> entries{};

                    SD_JOURNAL_FOREACH_BACKWARDS(xlog::journald::g_journal_handle) {
                        char* current_cursor{nullptr};
                        sd_journal_get_cursor(xlog::journald::g_journal_handle, &current_cursor);

                        if (std::string(initial_cursor) == std::string(current_cursor)) {
                            break;
                        }

                        std::pair<int64_t, std::string> result{};

                        if (!xlog::journald::journal_entry_procedure(xlog::journald::g_journal_handle, result)) {
                            continue;
                        }

                        entries.push_back(std::make_tuple(identifier, result.first, result.second));
                    }

                    for (auto entry{entries.rbegin()}; entry != entries.rend(); entry++) {
                        auto value{*entry};

                        if (config::field_verbose) {
                            debug::print("log-journald", "journal message recevived, details: '%s'", std::get<2>(value).c_str());
                        }

                        xlog::queue::insert(value);
                    }
                }
            }
        }
    #endif

    #ifdef _WIN32
        bool start(std::string identifier) {
            (void)(identifier);

            return false;
        }
    #else
        bool start(std::string identifier) {
            auto result{sd_journal_open(&xlog::journald::g_journal_handle, SD_JOURNAL_LOCAL_ONLY)};

            if (result < 0) {
                debug::print("log-journal", "failed to open the journal, error: %s", std::strerror(result));

                return false;
            }

            xlog::journald::g_worker_routine = std::make_optional(xlog::journald::worker(identifier));

            debug::print("log-journal", "started");

            return true;
        }
    #endif
    
        bool platform_support() {
        #ifdef _WIN32
            return false;
        #else
            return true;
        #endif
        }
    }
}
