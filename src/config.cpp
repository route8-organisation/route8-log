#include <exception>
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"
#include "config.hpp"
#include "debug.hpp"

namespace config {
    static const char* g_filename{"config.yml"};

    bool field_verbose{false};
    int64_t field_dispatch_sleep_ms{2000};

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
        }

        return true;
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
                return false; \
            } \
        }

        LOAD_CONFIG_KEY_VALUE("verbose", config::field_verbose);
        LOAD_CONFIG_KEY_VALUE("dispatch_sleep_ms", config::field_dispatch_sleep_ms);

        #undef LOAD_CONFIG_KEY_VALUE

        return true;
    }
}
