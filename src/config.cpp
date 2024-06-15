#include <exception>
#include "yaml-cpp/yaml.h"
#include "config.hpp"
#include "debug.hpp"

namespace config {
    static const char* g_filename{"config.yml"};

    bool        field_verbose{};
    int64_t     field_dispatch_sleep_ms{};
    size_t      field_maximum_log_entries{};
    int64_t     field_seconds_between_connects{};
    std::string field_remote_address{};
    uint16_t    field_remote_port{};
    std::string field_remote_certificate{};
    std::string field_identity{};
    std::string field_identity_password{};

    template<typename T>
    static bool load_config_key(YAML::Node& config, const char* key_name, T& value) {
        if (config[key_name]) {
            if (!config[key_name].IsScalar()) {
                debug::print("config", "key '{}' is not key-value", key_name);

                return false;
            }

            try {
                value = config[key_name].as<T>();
            } catch (const std::exception& e) {
                debug::print("config", "failed to load key '{}', error: {}", key_name, e.what());

                return false;
            }

            return true;
        }

        return false;
    }

    bool initialize() {
        debug::print("config", "initializing");

        YAML::Node config{};

        try {
            config = YAML::LoadFile(g_filename);
        } catch (const std::exception& e) {
            debug::print("config", "failed to load config file '{}', error: {}", g_filename, e.what());
            return false;
        }

        #define LOAD_CONFIG_KEY_VALUE(KEY_NAME, VARIABLE) { \
            if (!load_config_key(config, KEY_NAME, VARIABLE)) { \
                debug::print("config", "failed to load key '{}'", KEY_NAME); \
                return false; \
            } \
        }

        LOAD_CONFIG_KEY_VALUE("verbose", config::field_verbose);
        LOAD_CONFIG_KEY_VALUE("dispatch_sleep_ms", config::field_dispatch_sleep_ms);
        LOAD_CONFIG_KEY_VALUE("maximum_log_entries", config::field_maximum_log_entries);
        LOAD_CONFIG_KEY_VALUE("seconds_between_connects", config::field_seconds_between_connects);
        LOAD_CONFIG_KEY_VALUE("remote_address", config::field_remote_address);
        LOAD_CONFIG_KEY_VALUE("remote_port", config::field_remote_port);
        LOAD_CONFIG_KEY_VALUE("remote_certificate", config::field_remote_certificate);
        LOAD_CONFIG_KEY_VALUE("identity", config::field_identity);
        LOAD_CONFIG_KEY_VALUE("identity_password", config::field_identity_password);

        #undef LOAD_CONFIG_KEY_VALUE

        return true;
    }
}
