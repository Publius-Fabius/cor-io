#ifndef CORIO_COROUTINE_H
#define CORIO_COROUTINE_H

#include <coroutine>
#include <exception>

namespace corio
{
    using std::coroutine_handle;

    template <typename P> struct coroutine : coroutine_handle<P> {
        using promise_type = P;
        coroutine(coroutine_handle<P> h) : coroutine_handle<P>(h) { }
        struct awaiter {
            coroutine<P> callee;
            bool await_ready() { 
                return false; 
            }
            template<typename caller_type>
            coroutine_handle<> await_suspend(caller_type caller) {
                callee.promise().continuation = caller;
                callee.promise().state = caller.promise().state;
                return callee; 
            }
            auto await_resume() { 
                return std::move(callee.promise().result); 
            }
        };
        auto operator co_await() & {
            return awaiter{*this};
        }
        struct destructive_awaiter : awaiter {
            auto await_resume() { 
                auto result = std::move(this->callee.promise().result); 
                this->callee.destroy();
                return result;
            }
        };
        auto operator co_await() && {
            return destructive_awaiter{*this};
        }
    };

    template <typename S, typename A> struct promise {

        using state_type = S;
        using return_type = A;
        using coroutine_type = coroutine<promise<S,A>>;

        coroutine_handle<> continuation = nullptr;
        S *state;
        A result;

        auto get_return_object() {
            return coroutine_type::from_promise(*this);
        }

        auto initial_suspend() noexcept {
            return std::suspend_always();
        }

        struct final_awaiter {
            bool await_ready() noexcept { 
                return false; 
            }
            coroutine_handle<> await_suspend(coroutine_type h) noexcept {
                if(h.promise().continuation) 
                    return h.promise().continuation;
                return std::noop_coroutine();
            }
            void await_resume() noexcept { };
        };

        auto final_suspend() noexcept {
            return final_awaiter();
        }

        void return_value(A value) { 
            result = std::move(value); 
        }

        void unhandled_exception() {
            std::terminate();
        }
    };   
}

#endif