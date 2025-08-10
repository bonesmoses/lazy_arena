#include "lazy_alloc.h"
#include <stdio.h>

void nested_function() {
    uint8_t lazy = create_lazy_context(512);

    int* data = lazy_alloc(sizeof(int));
    if (data)
      *data = 42;

    printf("Nested allocated: %d\n", data ? *data : -1);

    //destroy_lazy_context(lazy); // OH NO, I FORGOT TO DEALLOCATE!!11!
}

int thread_func(void* arg) {
    uint8_t lazy = create_lazy_context(1024);

    int* data = lazy_alloc(sizeof(int));
    if (data)
        *data = 100;

    printf("Thread allocated: %d\n", data ? *data : -1);

    nested_function(); // Uses a nested arena

    // Original arena is still active

    data = lazy_alloc(sizeof(int));
    if (data)
        *data = 200;

    printf("Thread after nested: %d\n", data ? *data : -1);

    destroy_lazy_context(lazy); // Free context 0 *and* 1
}

int main() {
    thrd_t thread;
    thrd_create(&thread, thread_func, NULL);
    thrd_join(thread, NULL);
    return 0;
}
