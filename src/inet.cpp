#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "debug.hpp"
#include "config.hpp"
#include "nlohmann/detail/input/json_sax.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

namespace inet {
    static std::optional<std::reference_wrapper<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>> g_ssl_stream = std::nullopt;

    static bool g_connected{false};
    std::mutex  g_connection_fault_mutex{};
    static std::condition_variable g_connection_fault{};

    static bool send(std::string& data) {
        if (!inet::g_connected || !inet::g_ssl_stream.has_value()) {
            return false;
        }

        auto& ssl_stream{inet::g_ssl_stream.value().get()};

        std::vector<char> buffer(data.begin(), data.end());
        buffer.push_back(0);

        boost::system::error_code ec;
        boost::asio::write(ssl_stream, boost::asio::buffer(buffer), ec);

        if (ec) {
            debug::print("inet", "failed to send due to {}", ec.message());
            return false;
        }

        return true;
    }

    static bool receive(std::string& data) {
        if (!inet::g_connected || !inet::g_ssl_stream.has_value()) {
            return false;
        }

        auto& ssl_stream{inet::g_ssl_stream.value().get()};
        std::string result_buffer{};

        while (true) {
            boost::system::error_code ec;
            boost::asio::streambuf streambuf;
            boost::asio::read(ssl_stream, streambuf, ec);

            if (ec && ec != boost::asio::error::eof && ec != boost::asio::ssl::error::stream_truncated) {
                debug::print("inet", "failed to receive due to {}", ec.message());
                inet::g_connection_fault.notify_all();

                return false;
            }

            std::string buffer(boost::asio::buffer_cast<const char*>(streambuf.data()), streambuf.size());
            auto eof{buffer.find("\0")};

            if (eof == std::string::npos) {
                if (result_buffer.length() + buffer.length() >= config::field_maximum_receive_size) {
                    debug::print("inet", "received passed the receive limit");

                    return false;
                }

                result_buffer += buffer;
            } else {
                if (result_buffer.length() + eof >= config::field_maximum_receive_size) {
                    debug::print("inet", "received passed the receive limit");

                    return false;
                }

                result_buffer += std::string(buffer.begin(), buffer.begin() + (eof - 1));
                break;
            }
        }

        data = result_buffer;

        return true;
    }

    bool send_log(std::string log_identifier, int64_t log_timestamp, std::string log_data) {
        if (!inet::g_connected || !inet::g_ssl_stream.has_value()) {
            return false;
        }

        nlohmann::json data_json = {
            {"command", "log"},
            {"identity", config::field_identity},
            {"data", {
                {"identifier", log_identifier},
                {"timestamp", log_timestamp},
                {"message", log_data},
            }},
        };

        auto data_json_str{data_json.dump()};

        if (!inet::send(data_json_str)) {
            debug::print("inet", "failed to send log");
            inet::g_connection_fault.notify_all();

            return false;
        }

        return true;
    }

    static bool client_authenticate() {
        nlohmann::json authenticate_json = {
            {"command", "auth"},
            {"identity", config::field_identity},
            {"data", {
                {"password", config::field_identity_password},
            }},
        };

        std::string authenticate_json_str{authenticate_json.dump()};

        if (!inet::send(authenticate_json_str)) {
            debug::print("inet", "failed to authenticate");

            return false;
        }

        std::string server_result_str{};

        if (!inet::receive(server_result_str)) {
            debug::print("inet", "failed to authenticate");

            return false;
        }

        try {
            auto server_result{nlohmann::json::parse(server_result_str)};

            if (server_result.contains("auth") && server_result["auth"] == "authenticated") {
                return true;
            }
        } catch (const std::exception& e) {
            debug::print("inet", "failed to authenticate due to exception: {}", e.what());
        }

        return false;
    }

    static void client_procedure() {
        if (!inet::client_authenticate()) {
            return;
        }

        debug::print("inet", "authenticated");

        std::unique_lock connection_fault_lock(inet::g_connection_fault_mutex);
        inet::g_connection_fault.wait(connection_fault_lock);
    }

    bool connect() {
        while (true) {
            boost::asio::io_context io_context;

            if (!std::filesystem::exists(config::field_remote_certificate)) {
                debug::print("inet", "missing PEM '{}'", config::field_remote_certificate);

                return false;
            }

            try {
                debug::print("inet", "connecting to '{}:{}' with PEM '{}'", config::field_remote_address, config::field_remote_port, config::field_remote_certificate);

                boost::asio::ip::tcp::resolver resolver(io_context);
                auto endpoints{resolver.resolve(config::field_remote_address, std::to_string(config::field_remote_port))};

                for (auto endpoint : endpoints) {
                    auto remote_address{endpoint.endpoint().address().to_string()};
                    auto remote_port{endpoint.endpoint().port()};

                    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
                    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_stream(io_context, ctx);

                    ctx.set_default_verify_paths();

                    try {
                        ssl_stream.lowest_layer().connect(endpoint);
                        ssl_stream.handshake(boost::asio::ssl::stream_base::client);
                    } catch (const std::exception& e) {
                        debug::print("inet", "failed to connect to {}:{}", remote_address, remote_port);
                        continue;
                    }

                    debug::print("inet", "connected to {}:{}", remote_address, remote_port);

                    inet::g_ssl_stream = ssl_stream;
                    inet::g_connected = true;
                    inet::client_procedure();
                    inet::g_connected = false;
                    inet::g_ssl_stream = std::nullopt;
                    debug::print("inet", "stream closed");

                    break;
                }
            } catch (const std::exception& e) {
                debug::print("inet", "exception occured: {}; trying again after 10 seconds", e.what());
                inet::g_connected = false;
            }

            debug::print("inet", "waiting {} seconds till next attempt", config::field_seconds_between_connects);
            std::this_thread::sleep_for(std::chrono::seconds(config::field_seconds_between_connects));
        }
    }
}