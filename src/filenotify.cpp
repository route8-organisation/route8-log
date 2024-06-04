#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <format>
#include "config.hpp"
#include "debug.hpp"
#include "filenotify.hpp"

#ifdef _WIN32

filenotify::filenotify(const std::string filepath) {
    (void)(filepath);
}

filenotify::~filenotify() {

}

#else
#include <unistd.h>
#include <sys/inotify.h>

bool filenotify::wait() {
    char buffer[4096];

    while (true) {
        auto read_length{::read(this->handle, buffer, sizeof(buffer))};

        if (read_length == -1 && errno != EAGAIN) {
            debug::print("filenotify", "failed to read from handle, error: {}", std::strerror(errno));
            return false;
        }

        if (!read_length) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        for (char* ptr{buffer}; ptr < buffer + read_length; ptr += sizeof(inotify_event) + reinterpret_cast<inotify_event*>(buffer)->len) {
            const inotify_event* event{reinterpret_cast<inotify_event*>(ptr)};

            if (this->filename == event->name) {
                if (config::field_verbose) {
                    debug::print("filenotify", "detected modification on '{}'", this->full_path);
                }

                break;
            }
        }
    }

    return true;
}

filenotify::filenotify(const std::string filepath) {
    std::filesystem::path path(filepath);
    std::string directory{path.parent_path()};
    this->filename = path.filename();
    this->full_path = filepath;

    debug::print("filenotify", "setting watch in folder '{}' for file '{}'", directory, this->filename);

    this->handle = inotify_init1(IN_NONBLOCK);

    if (this->handle == filenotify_handle_nullptr) {
        throw std::runtime_error("failed to create a handle");
    }

    this->file_handle = inotify_add_watch(this->handle, directory.c_str(), IN_CLOSE_WRITE | IN_MODIFY);

    if (this->file_handle == filenotify_handle_nullptr) {
        ::close(this->handle);
        throw std::runtime_error(std::format("failed to add the file '{}' to the watch list", filepath));
    }

}

filenotify::~filenotify() {
    if (this->handle != filenotify_handle_nullptr) {
        ::close(this->handle);
        this->handle = filenotify_handle_nullptr;
    }
}

#endif
