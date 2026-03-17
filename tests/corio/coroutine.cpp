#include "corio/coroutine.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

using namespace corio;

struct my_state {
    int value;
};

template <typename R>
using my_pro = promise<my_state, R>;

template<typename R>
using my_cor = coroutine<my_pro<R>>;

template<typename R>
struct con_test {
    my_cor<R> cor;
    con_test(my_cor<R> &&cor_) : 
        cor(std::move(cor_)) { }
};

int main(int argc, char **args) {
    puts("testing coroutines...");

    auto c1 = ([]() -> my_cor<int> {
        puts("inside f1");
        co_return 5;
    })();

    puts("resuming c1");

    c1.handle.resume();
    assert(c1.handle.promise().result == 5);
    assert(c1.handle.done());
    
    my_state state{ 123 };
    my_state *state_ptr = &state;

    puts("creating c2");
    auto c2 = ([state_ptr]() -> my_cor<char> {
        puts("inside c2");

        auto c3 = ([state_ptr]() -> my_cor<int> {
            puts("inside c3");
            auto *ptr = co_await get_state<my_state>();
            assert(ptr == state_ptr);
            co_return 3;
        })();
        int result = co_await c3;
        assert(result == 3);
        auto *ptr = co_await get_state<my_state>();
        assert(ptr == state_ptr);

        auto c4 = ([]() -> my_cor<int> {
            puts("inside c4");
            co_return 4;
        });
        result = co_await c4();
        assert(result == 4);

        result = co_await ([]() -> my_cor<int> {
            puts("inside f5");
            int x = 5;
            co_return x; // can co_return accept lvalue?
        })();
        assert(result == 5);
        
        puts("testing coroutine move constructor");
        con_test<int> tt(([]() -> my_cor<int> {
            puts("inside con_test");
            puts("inside f6");
            co_return 6;
        })());
        puts("outside con_test");
        result = co_await tt.cor;
        assert(result == 6);
        
        co_return 'x';
    })();

    c2.handle.promise().state = &state;
    c2.handle.resume();
    assert(c2.handle.promise().result == 'x');
    assert(c2.handle.done());
 
    return EXIT_SUCCESS;
}

