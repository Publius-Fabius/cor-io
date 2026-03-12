#ifndef CORIO_COROUTINE_H
#define CORIO_COROUTINE_H

#include <coroutine>
#include <exception>

namespace corio
{
    struct coroutine_promise;

    struct coroutine : std::coroutine_handle<coroutine_promise> 
    {
        // awaiter functions 
        // bool await_ready() const noexcept;
        // void await_suspend(coroutine handle);
        // int await_resume() noexcept;
    };

    struct coroutine_promise
    {
        coroutine get_return_object();
        std::suspend_always initial_suspend() noexcept;
        std::suspend_always final_suspend() noexcept;
        void return_void();
        void unhandled_exception();
    };

    coroutine coroutine_promise::get_return_object() 
    { 
        return { coroutine::from_promise(*this) };
    }

    std::suspend_always coroutine_promise::initial_suspend() noexcept 
    { 
        return { }; 
    }

    std::suspend_always coroutine_promise::final_suspend() noexcept 
    { 
        return { }; 
    }

    void coroutine_promise::return_void() 
    {
        return;
    }

    void coroutine_promise::unhandled_exception() 
    {
        std::terminate();
    }
}

#endif