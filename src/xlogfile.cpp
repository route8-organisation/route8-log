#include <exception>
#include <fstream>
#include <future>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "debug.hpp"
#include "config.hpp"
#include "filenotify.hpp"
#include "xlog.hpp"

namespace xlog {
    namespace file {
        static std::vector<std::future<void>> g_worker_list{};

        static void worker(std::string identifier, std::string source_filename) {
            auto get_source_file_size = [source_filename]() -> std::streampos {
                std::ifstream stream(source_filename, std::ios_base::in | std::ios_base::ate);
                return stream.tellg();
            };

            try {
                std::streampos position{get_source_file_size()};

                filenotify filenotify(source_filename);

                while (true) {
                    if (!filenotify.wait()) {
                        break;
                    }

                    std::streampos new_position_check{get_source_file_size()};

                    if (new_position_check < position) {
                        position = new_position_check;
                    }

                    std::ifstream file_stream(source_filename, std::ios_base::in);

                    if (file_stream.is_open()) {
                        if (!file_stream.seekg(position)) {
                            file_stream.clear();
                            file_stream.seekg(0);

                            debug::print("log-file", "failed to seek to previus position, setting to 0");
                        }

                        int chr{0};
                        std::string blob{};

                        while (true) {
                            chr = file_stream.get();

                            if (file_stream.fail() || file_stream.eof()) {
                                break;
                            }

                            if (!chr) { continue; }
                            if (chr == '\r') { continue; }

                            blob.push_back(static_cast<char>(chr));
                        }

                        file_stream.clear();
                        position = file_stream.tellg();

                        if (!blob.empty()) {
                            if (!blob.ends_with('\n')) {
                                blob.push_back('\n');
                            }

                            std::string line{};
                            std::stringstream blob_stream(blob);

                            while (std::getline(blob_stream, line)) {
                                if (config::field_verbose) {
                                    debug::print("log-file", "detected line from '{}': '{}'", source_filename, line);
                                }

                                auto timestamp{static_cast<int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
                                xlog::queue::insert(std::make_tuple(identifier, timestamp, line));
                            }
                        }

                    } else {
                        debug::print("log-file", "failed to open file '{}' for reading", source_filename);
                        return;
                    }
                }
            } catch (const std::exception& e) {
                debug::print("log-file", "error occured while working on file '{}'; error: {}; stopping worker", source_filename, e.what());
            }

            (void)(identifier);
        }

        bool start(std::string identifier, std::string source_filename) {
            xlog::file::g_worker_list.push_back(std::async(xlog::file::worker, identifier, source_filename));
            debug::print("log-file", "started on file '{}'", source_filename);

            return false;
        }
    }
}
