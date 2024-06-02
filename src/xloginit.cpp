#include <exception>
#include <functional>
#include <string>
#include <unordered_map>
#include "xlog.hpp"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/yaml.h"
#include "debug.hpp"

namespace xlog {
    static const char* g_filename{"log.yml"};

    struct LogEntry {
        const std::function<bool(const YAML::Node&)> check;
        const std::function<bool(const YAML::Node&)> setup;
    };

    static const std::unordered_map<std::string, LogEntry> g_lookup_table = {
        { "journald", {
            .check = [](const YAML::Node& config) -> bool {
                if (!xlog::journald::platform_support()) {
                    debug::print("log", "journal is not supported on this platform");

                    return false;
                }

                if (!config["identifier"]) {
                    debug::print("log", "journal entry is missing key identifier");

                    return false;
                }

                if (!config["identifier"].IsScalar()) {
                    debug::print("log", "journal entry is not key-value type");

                    return false;
                }

                return true;
            },
            .setup = [](const YAML::Node& config) -> bool {
                std::string identifier{};

                try {
                    identifier = config["identifier"].as<std::string>();
                } catch (const std::exception& e) {
                    debug::print("log", "failed to load key 'identifier', error: {}", e.what());

                    return false;
                }

                return xlog::journald::start(identifier);
            },
        }}
    };

    /* checks for invalid config */
    static bool check_config(const YAML::Node& config) {
        for (auto&& entry : config) {
            const auto key_name{entry.first.as<std::string>()};

            auto lookup_entry{xlog::g_lookup_table.find(key_name)};

            if (lookup_entry == xlog::g_lookup_table.end()) {
                debug::print("log", "unknown logging type '{}'", key_name);

                return false;
            }

            const auto check_fn = xlog::g_lookup_table.at(key_name).check;

            if (!check_fn(entry.second.as<YAML::Node>())) {
                return false;
            }
        }

        return true;
    }

    static bool run_config(const YAML::Node& config) {
        for (auto&& entry : config) {
            const auto key_name{entry.first.as<std::string>()};

            auto lookup_entry{xlog::g_lookup_table.find(key_name)};

            if (lookup_entry == xlog::g_lookup_table.end()) {
                debug::print("log", "unknown logging type '{}'", key_name);

                return false;
            }

            const auto setup_fn = xlog::g_lookup_table.at(key_name).setup;

            if (!setup_fn(entry.second.as<YAML::Node>())) {
                return false;
            }
        }

        return true;
    }

    bool initialize() {
        YAML::Node config{};

        try {
            config = YAML::LoadFile(xlog::g_filename);
        } catch (const std::exception& e) {
            debug::print("log", "failed to load config file '{}', error: {}", xlog::g_filename, e.what());
            return false;
        }

        if (!xlog::check_config(config)) {
            return false;
        }

        if (!xlog::run_config(config)) {
            return false;
        }

        return true;
    }
}
