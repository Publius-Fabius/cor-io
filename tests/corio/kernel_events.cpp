#include "corio/kernel_events.h"
#include <stdio.h>
#include <unistd.h>

using namespace corio;

void create_destroy() {
    kernel_events events(16);
    int fds[2];
    pipe(fds);
    event_fd reader(event_fd::unique(), fds[0]);
    event_fd writer(event_fd::unique(), fds[1]);
}

int main(int argc, char **args) {
    puts("testing kernel_events...");

    create_destroy();
}