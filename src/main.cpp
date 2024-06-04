#include <chrono>
#include <iostream>
#include <thread>
#include "debug.hpp"
#include "config.hpp"
#include "xlog.hpp"

#include "filenotify.hpp"

int main() {
    if (!debug::initialize()) {
        std::cerr << "failed to initialize the debug module; aborting!";

        return -1;
    }

    debug::print("app", "############## STARTED ##############");

    if (!config::initialize()) {
        return -2;
    }

    if (!xlog::queue::start()) {
        return -3;
    }

    if (!xlog::initialize()) {
        return -4;
    }

    for (;;) {std::this_thread::sleep_for(std::chrono::seconds(10));}

    // return 0;
}
