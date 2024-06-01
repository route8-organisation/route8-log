#ifndef __COROUTINE_HPP
#define __COROUTINE_HPP

#include "debug.hpp"
#include <coroutine>
#include <thread>

struct promise;
 
struct routine:std::coroutine_handle<promise> {
    using promise_type = ::promise;
};
 
struct promise {
    routine get_return_object() { return {routine::from_promise(*this)};}
    auto initial_suspend() noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept { return std::suspend_never{}; }
    void return_void() {}
    void unhandled_exception() {
        debug::print("coroutine-core", "unhandled exception");
        std::terminate();
    }
};

template<typename T>
struct sleep_routine {
    T duration{};

    bool await_ready() const {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([this, handle]() {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }

    void await_resume() const {}
};

#endif
