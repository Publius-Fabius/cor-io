#include "corio/slot_map.h"
#include <stdio.h>
#include <stdbool.h>
#include <set>

using namespace corio;

void test_acquire_release() {
    slot_map<int> m;

    int ints[] = { 1, 2, 3, 4, 5 };
    int slots[] = { -1, -2, -3, -4, -5 };

    assert(m.size() == 0);
    slots[0] = m.acquire(ints);
    assert(m.size() == 1);
    slots[1] = m.acquire(ints + 1);
    assert(m.size() == 2);
    slots[2] = m.acquire(ints + 2);
    assert(m.size() == 3);
    slots[3] = m.acquire(ints + 3);
    assert(m.size() == 4);
    slots[4] = m.acquire(ints + 4);
    assert(m.size() == 5);

    auto ismember = [&m](int *ptr) -> bool {
        bool result = false;
        for(int *i : m) if(i == ptr) result = true;
        return result;
    };

    auto noduplicates = [&slots]() -> bool {
        std::set<int> c;
        for(int s : slots) {
            if(c.contains(s)) return false;
            c.insert(s);
        }
        return true;
    };

    m.release(slots[2]);
    assert(m.size() == 4);
    assert(!ismember(ints + 2));
    assert(noduplicates());

    m.release(slots[0]);
    assert(m.size() == 3);
    assert(!ismember(ints));
    assert(noduplicates());

    m.release(slots[4]);
    assert(m.size() == 2);
    assert(!ismember(ints + 4));
    assert(noduplicates());

    assert(ismember(ints + 1));
    assert(ismember(ints + 3));
    assert(noduplicates());

    slots[2] = m.acquire(ints + 2);
    assert(ismember(ints + 2));

    slots[4] = m.acquire(ints + 4);
    assert(ismember(ints + 4));

    slots[0] = m.acquire(ints + 0);
    assert(ismember(ints));
    assert(noduplicates());

    m.release(slots[1]);
    assert(!ismember(ints + 1));
    slots[1] = m.acquire(ints + 1);
    assert(ismember(ints + 1));
    assert(noduplicates());
}

int main(int argc, char **args) {
    puts("testing slot_map...");

    test_acquire_release();
}