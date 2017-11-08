#include <stdio.h>
#include <stdlib.h>

volatile int wait_for_debugger = 1;

extern "C" void rust_init(int argc, char **argv);
extern "C" void rust_deinit(int status, void *arg);
extern "C" void dummy_func() {
}

extern "C" __attribute__((constructor)) void host_init(int argc, char **argv) {
    if(getenv("M3_WAIT") != 0) {
        while(wait_for_debugger != 0) {
        }
    }

    rust_init(argc, argv);
    on_exit(rust_deinit, nullptr);
}