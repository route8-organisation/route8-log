cmake_minimum_required(VERSION 3.20)

add_subdirectory(submodule/yaml-cpp)
add_subdirectory(submodule/nlohmann-json)

project(route8-log VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    src/main.cpp
    src/debug.cpp
    src/config.cpp
    src/xloginit.cpp
    src/xlogqueue.cpp
    src/xlogjournald.cpp
)

add_executable(route8-log ${SOURCES})

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(route8-log PRIVATE -Wall -Wextra -Wpedantic -Werror)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(route8-log PRIVATE /W4 /WX)
endif()

target_include_directories(route8-log PUBLIC
    submodule/yaml-cpp/include/
    submodule/nlohmann-json/include/
)
target_link_libraries(route8-log yaml-cpp systemd)
