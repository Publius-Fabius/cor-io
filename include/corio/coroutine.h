#ifndef CORIO_COROUTINE_H
#define CORIO_COROUTINE_H

#include <coroutine>
#include <exception>
#include <utility>
#include <stdio.h>

namespace corio
{
    using std::coroutine_handle;

    /**
     * A move-only RAII wrapper for a C++20 coroutine handle. This template 
     * manages the lifetime of a coroutine state. It ensures that the 
     * coroutine is destroyed when the wrapper goes out of scope and provides 
     * an awaiter to allow the coroutine to be used within co_await 
     * expressions.
     */
    template <typename P> struct coroutine {
        
        /** Coroutine's Promise Type */
        using promise_type = P;

        /** Coroutine Handle Type */
        using handle_type = coroutine_handle<P>;
        
        /** Underlying Coroutine Handle */
        coroutine_handle<P> handle;

        /**
         * Constructs a coroutine from an existing coroutine handle.
         */
        explicit coroutine(coroutine_handle<P> handle_) : 
            handle(handle_) { }

        /**
         * Move constructor. Transfers ownership of the coroutine handle.
         */
        coroutine(coroutine<P> &&other) : 
            handle(std::exchange(other.handle, nullptr)) { }

        /**
         * Destroys the coroutine state if the handle is valid.
         */
        ~coroutine() {
            if(handle) handle.destroy();
        }

        // Delete copy operations to ensure strict single-ownership (RAII).

        coroutine(coroutine<P> &) = delete;
        coroutine(const coroutine<P>&) = delete;
        coroutine& operator=(coroutine<P>&) = delete;
        coroutine& operator=(const coroutine<P>&) = delete;
        
        /**
         * Internal awaiter used to suspend the caller and resume this 
         * coroutine.
         */
        struct awaiter {

            /** Reference to the coroutine being awaited. */
            coroutine<P> &callee;

            /** Always returns false to force the caller to suspend. */
            bool await_ready() { 
                return false; 
            }

            /**
             * Links the caller and callee promises and transfers execution.
             * @param caller The handle of the calling coroutine.
             * @return The handle of the callee to be resumed.
             */
            template<typename caller_handle>
            coroutine_handle<> await_suspend(caller_handle &caller) {

                // Set up the state continuation
                callee.handle.promise().continuation = caller;
                callee.handle.promise().state = caller.promise().state;

                // Transfer execution to the callee.
                return callee.handle; 
            }

            /** Returns the result stored in the callee's promise. */
            auto await_resume() { 
                return std::move(callee.handle.promise().result); 
            }
        };

        /**
         * @brief Enables the use of co_await on this coroutine object.
         * @return An awaiter instance.
         */
        auto operator co_await() {
            return awaiter{*this};
        }
    };

    /**
     * The promise object that directs the behavior of the coroutine. This 
     * class handles the lifecycle events of the coroutine, including its 
     * initial suspension, how it returns values, and how it transfers control
     * back to a caller upon completion.
     * @tparam S The state type (context) shared across the coroutine chain.
     * @tparam A The return value type of the coroutine.
     */
    template <typename S, typename A> struct promise {

        /** Alias for the shared state type. */
        using state_type = S;

        /** Alias for the value type returned via co_return. */
        using return_type = A;

        /** The associated coroutine type. */
        using coroutine_type = coroutine<promise<S,A>>;
        
        /** The handle of the parent coroutine to resume upon completion. */
        coroutine_handle<> continuation = nullptr;

        /** Pointer to the shared execution state/context. */
        S *state;

        /** Storage for the value produced by co_return. */
        A result;

        /**
         * Returns a coroutine object owning the handle to this promise.
         */
        coroutine<promise<S,A>> get_return_object() {
            return coroutine(
                coroutine_handle<promise<S,A>>::from_promise(*this));
        }

        /**
         * Returns std::suspend_always to ensure the coroutine does not start 
         * immediately upon creation (lazy execution).
         */
        auto initial_suspend() noexcept {
            return std::suspend_always();
        }

        /**
         * Custom awaiter used during the coroutine's final suspension phase.
         */
        struct final_awaiter {

            /** Always returns false to force suspension. */
            bool await_ready() noexcept { 
                return false; 
            }

            /**
             * Transfers execution back to the caller (Symmetric Transfer).
             * @param h The handle of the coroutine that just finished.
             * @return The continuation handle to resume, or 
             * std::noop_coroutine() if no continuation is set.
             */
            template<typename handle>
            coroutine_handle<> await_suspend(handle &h) noexcept {
                if(h.promise().continuation) 
                    return h.promise().continuation;
                return std::noop_coroutine();
            }

            /** No-op; coroutine state is cleaned by coroutine::awaiter  */
            void await_resume() noexcept { };
        };

        /**
         * Returns a final_awaiter to manage symmetric transfer to the caller.
         */
        auto final_suspend() noexcept {
            return final_awaiter();
        }

        /** Captures the value provided by the co_return statement. */
        void return_value(A &&value) { 
            result = std::move(value); 
        }

        /** 
         * Global error handler for the coroutine. Currently calls 
         * std::terminate() for any unhandled exception.
         */
        void unhandled_exception() {
            std::terminate();
        }
    };

    /**
     * A utility awaiter used to retrieve the shared state pointer from the 
     * current coroutine.
     */
    template<typename S>
    struct get_state {

        /** Internal storage for the state pointer. */
        S *result = nullptr;

        /** Always returns false to force suspension. */
        bool await_ready() const noexcept { 
            return false; 
        }

        /**
         * Extracts the state pointer from the coroutine's promise. Because 
         * this returns false, the coroutine does not actually stay suspended;
         * it immediately resumes after the pointer has been captured.
         */
        template<typename P>
        bool await_suspend(std::coroutine_handle<P> h) noexcept {
            result = h.promise().state; 
            return false;
        }

        /**
        * Returns the captured state pointer to the co_await expression.
        */
        S* await_resume() const noexcept { 
            return result; 
        }
    };
}

#endif