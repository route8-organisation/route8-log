#ifdef _WIN32
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <format>
#include <locale>
#include <codecvt>
#include "config.hpp"
#include "debug.hpp"
#include "filenotify.hpp"

#ifdef _WIN32
bool filenotify::wait() {
    while (true){
        constexpr DWORD buffer_length{4096};
        BYTE buffer[buffer_length]{};
        DWORD bytes_returned{0};

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide_filename{converter.from_bytes(this->filename)};

        auto read_result{ReadDirectoryChangesW(
            this->handle,
            &buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes_returned,
            NULL,
            NULL)
        };

        if (!read_result) {
            debug::print("filenotify", "failed to read directory changes, error: 0x{:08X}", GetLastError());
            return false;
        }

        FILE_NOTIFY_INFORMATION* fni{reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer)};

        do {
            std::wstring affected_filename(fni->FileName, fni->FileNameLength / sizeof(WCHAR));

            if (wide_filename == affected_filename) {
                if (fni->Action == FILE_ACTION_MODIFIED) {
                    return true;
                }
            }

            if (fni->NextEntryOffset == 0) {
                break;
            }

            fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset);
        } while (true);
    }
}

filenotify::filenotify(const std::string filepath) {
    std::filesystem::path path(filepath);
    std::string directory{path.parent_path().string()};
    this->filename = path.filename().string();
    this->full_path = filepath;

    debug::print("filenotify", "setting watch in folder '{}' for file '{}'", directory, this->filename);

    this->handle = CreateFileA(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (this->handle == filenotify_handle_nullptr) {
        throw std::runtime_error("failed to create a handle");
    }
}

filenotify::~filenotify() {
    if (this->handle != filenotify_handle_nullptr) {
        CloseHandle(this->handle);
    }
}

#else
#include <unistd.h>
#include <sys/inotify.h>

bool filenotify::wait() {
    while (true) {
        char buffer[4096]{};
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
                return true;
            }
        }
    }
}

filenotify::filenotify(const std::string filepath) {
    std::filesystem::path path(filepath);
    std::string directory{path.parent_path().string()};
    this->filename = path.filename().string();
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
