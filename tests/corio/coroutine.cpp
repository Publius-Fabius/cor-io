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

int main(int argc, char **args) {
    puts("testing coroutines...");

    auto f1 = []() -> my_cor<int> {
        puts("inside f1");
        co_return 5;
    };

    puts("creating c1");
    auto c1 = f1();

    puts("resuming c1");

    c1.resume();
    assert(c1.promise().result == 5);
    assert(c1.done());
    c1.destroy();
    
    auto f2 = [f1]() -> my_cor<char> {
        puts("inside f2");
        int result = co_await f1();
        assert(result == 5);
        co_return 'x';
    };
    
    puts("creating c2");
    auto c2 = f2();

    puts("resuming c2");
    c2.resume();
    assert(c2.promise().result == 'x');
    assert(c2.done());
    c2.destroy();

    return EXIT_SUCCESS;
}