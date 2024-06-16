
#ifdef _WIN32
#include <Windows.h>
#include <minwindef.h>
#include <winscard.h>
#include <winbase.h>
#include <handleapi.h>
#include <errhandlingapi.h>
#endif

#include <chrono>
#include <future>
#include <thread>
#include <optional>
#include <string>
#include <future>
#include <array>
#include <memory>
#include "debug.hpp"
#include "config.hpp"
#include "xlog.hpp"
#include "nlohmann/json.hpp"

namespace xlog {
    namespace winevent {
    #ifdef _WIN32
        static std::vector<std::future<void>> g_worker_routines{};

        static DWORD read_record(HANDLE log_event_handle, char*& result, DWORD record_numb, DWORD flags) {
            DWORD status = ERROR_SUCCESS;
            DWORD bytes_read{0};
            DWORD min_bytes_to_read{0};
            std::array<char, sizeof(EVENTLOGRECORD)> record_probe{};

            if (!ReadEventLog(log_event_handle, flags, record_numb, record_probe.data(), static_cast<DWORD>(record_probe.size()), &bytes_read, &min_bytes_to_read)) {
                status = GetLastError();

                if (status == ERROR_INSUFFICIENT_BUFFER){
                    status = ERROR_SUCCESS;

                    char* buffer{new char[min_bytes_to_read]};
                    bytes_read = min_bytes_to_read;

                    if (ReadEventLog(log_event_handle, flags, record_numb, buffer, static_cast<DWORD>(min_bytes_to_read), &bytes_read, &min_bytes_to_read)) {
                        result = buffer;
                    } else {
                        status = GetLastError();
                        debug::print("winevent", "failed to read an event record, error: 0x{:08X}", status);

                        delete[] buffer;
                    }
                } else {
                    if (status != ERROR_HANDLE_EOF) {
                        debug::print("winevent", "failed to read an event record, error: 0x{:08X}", status);
                    }
                }
            }

            return status;
        }

        static bool get_tail_record_offset(HANDLE event_log_handle, DWORD& record_offset) {
            DWORD oldest_record{0};
            DWORD newest_record{0};

            if (!GetOldestEventLogRecord(event_log_handle, &oldest_record)){
                debug::print("winevent", "failed to get the oldest record, error: 0x{:08X}", GetLastError());
                return false;
            }

            if (!GetNumberOfEventLogRecords(event_log_handle, &newest_record)){
                debug::print("winevent", "failed to get the number of records, error: 0x{:08X}", GetLastError());
                return false;
            }

            record_offset = oldest_record + newest_record - 1;

            return true;
        }

        static bool seek_tail(HANDLE event_log_handle) {
            DWORD tail_offset{0};
            char* record{nullptr};

            debug::print("winevent", "seeking to the end");

            if (!get_tail_record_offset(event_log_handle, tail_offset)) {
                debug::print("winevent", "failed to get the number of the last record");
                return false;
            }

            if (ERROR_SUCCESS != read_record(event_log_handle, record, tail_offset, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ)) {
                debug::print("winevent", "failed to seek to record {}", tail_offset);
                return false;
            }

            if (record) {
                delete[] record;
            }

            return true;
        }

        static std::string record_event_type_translate(DWORD value) {
            switch (value) {
            case EVENTLOG_ERROR_TYPE:
                return "ERROR";
            case EVENTLOG_WARNING_TYPE:
                return "WARNING";
            case EVENTLOG_INFORMATION_TYPE:
                return "INFORMATION";
            case EVENTLOG_AUDIT_SUCCESS:
                return "AUDIT_SUCCESS";
            case EVENTLOG_AUDIT_FAILURE:
                return "AUDIT_FAILURE";
            default:
                return "";
            }
        }

        static void worker_inside(HANDLE event_log_handle, HANDLE wait_event, std::string& identifier, std::string& log_source_name) {
            xlog::winevent::seek_tail(event_log_handle);

            while (true) {
                DWORD wait_status{WaitForSingleObject(wait_event, INFINITE)};

                if (wait_status == WAIT_OBJECT_0) {
                    DWORD read_status{ERROR_SUCCESS};
                    std::vector<xlog::queue::log_entry_t> entries{};

                    do {
                        char* record_buffer{nullptr};
                        read_status = read_record(event_log_handle, record_buffer, 0, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ);

                        if (read_status != ERROR_SUCCESS && read_status != ERROR_HANDLE_EOF) {
                            debug::print("winevent", "failed to read, error: 0x{:08X}", read_status);
                            return;
                        }

                        if (record_buffer) {
                            std::unique_ptr<char[]> record_buffer_safe(record_buffer);
                            EVENTLOGRECORD* record = reinterpret_cast<EVENTLOGRECORD*>(record_buffer_safe.get());
                            if (record->EventID) {
                                try {
                                    std::stringstream description{};
                                    const char* description_c{reinterpret_cast<const char*>(reinterpret_cast<size_t>(record) + record->StringOffset)};

                                    for (DWORD i = 0; i < record->NumStrings; i++) {
                                        description << description_c;

                                        if (i + 1 < record->NumStrings) {
                                            description << std::endl;
                                        }

                                        description_c += strlen(description_c) + 1;
                                    }

                                    nlohmann::json data{
                                        {"event_source", log_source_name},
                                        {"event_type", record_event_type_translate(record->EventType)},
                                        {"event_category", record->EventCategory},
                                        {"time_generated", record->TimeGenerated},
                                        {"time_written", record->TimeWritten},
                                        {"source_name", std::string(reinterpret_cast<const char*>(reinterpret_cast<size_t>(record) + sizeof(EVENTLOGRECORD)))},
                                        {"description", description.str()},
                                    };

                                    auto timestamp{static_cast<int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
                                    entries.push_back(std::make_tuple(identifier, timestamp, data.dump()));
                                } catch (const std::exception& e) {
                                    debug::print("winevent", "corrupted record, error: {}", e.what());
                                }
                            }
                        }
                    } while (read_status != ERROR_HANDLE_EOF);

                    for (auto entry{entries.rbegin()}; entry != entries.rend(); entry++) {
                        auto value{*entry};

                        if (config::field_verbose) {
                            debug::print("winevent", "event received, details: '{}'", std::get<2>(value));
                        }

                        xlog::queue::insert(value);
                    }
                }
            }

            (void)(identifier);
            (void)(log_source_name);
        }

        static void worker(std::string identifier, std::string log_source_name) {
            HANDLE event_log_handle{OpenEventLogA(NULL, log_source_name.c_str())};

            if (event_log_handle == NULL) {
                debug::print("winevent", "failed to open '{}' log, error: 0x{:08X}", log_source_name, GetLastError());
                return;
            }

            HANDLE wait_event{CreateEvent(NULL, TRUE, FALSE, NULL)};

            if (wait_event == NULL) {
                debug::print("winevent", "failed to create wait event for '{}' log, error: 0x{:08X}", log_source_name, GetLastError());
                CloseEventLog(event_log_handle);
                return;
            }

            if (!NotifyChangeEventLog(event_log_handle, wait_event)) {
                debug::print("winevent", "failed to set waiting event for '{}' log, error: 0x{:08X}", log_source_name, GetLastError());
                CloseHandle(wait_event);
                CloseEventLog(event_log_handle);
                return;
            }

            xlog::winevent::worker_inside(event_log_handle, wait_event, identifier, log_source_name);
            CloseEventLog(event_log_handle);
            CloseHandle(wait_event);
        }
    #endif

    #ifdef _WIN32
        bool start(std::string identifier, std::string source_name) {
            xlog::winevent::g_worker_routines.push_back(std::async(xlog::winevent::worker, identifier, source_name));
            debug::print("winevent", "started on source '{}'", source_name);

            return true;
        }
    #else
        bool start(std::string identifier, std::string source_name) {
            (void)(identifier);
            (void)(source_name);

            return false;
        }
    #endif

        bool platform_support() {
        #ifdef _WIN32
            return true;
        #else
            return false;
        #endif
        }
    }
}
