#include "corio/defer.h"
#include <stdio.h>
#include <system_error>

using namespace corio;

void test() {
    puts("testing defer...");

    defer(
        puts("deferred execution");
    )

    puts("execution");

    throw std::system_error(errno, std::generic_category(), "YIKES!");  
}

int main(int argc, char** args) {
    // defer only works if test is surrounded in try/catch.
    try{
        test();
    } catch(...) {

    }
}