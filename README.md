# The Laziest Arena Allocator

Do you code in C? Do you allocate memory to pointers? Are you incredibly---possibly even pathologically---lazy?

Well you're in luck! This library provides a memory allocation technique that is _made_ for people like you! Here's how it works:

1. Create a memory context
2. Allocate like you normally would
3. Reset the memory context
4. Repeat from 2 or
5. Destroy the memory context

"Wait," you might ask, "How is this different than just using malloc?"

A standard malloc call in a nested loop might look like this:

```c
int main() {
    for (int j = 0; j < 1000; j++) {
        for (int i = 0; i < 100; i++) {
            char *dur = malloc(sizeof(char) * 10);
            snprintf(dur, 10, "stuff");
            free(dur)
        }
    }
}
```

Did you think all those calls to `free` are... well, free? And what if you forget one in a longer and more complicated routine, call stack, or particularly dense struct, list, tree, or whatever? What if it was possible to just do this instead:

```c
int main() {
    uint8_t lazy = create_lazy_context(1000 * sizeof(char));

    for (int j = 0; j < 1000; j++) {
        for (int i = 0; i < 100; i++) {
            char *dur = lazy_alloc(sizeof(char) * 10);
            snprintf(dur, 10, "stuff");
        }
        reset_lazy_context();
    }
    destroy_lazy_context(lazy);
}
```

The library works by providing a fully nestable stack of Arenas where we essentially allocate one large chunk and then provide smaller allocations when requested. Each context is its own allocation, so routines can create their own scratch area and reset it as many times as they want, and throw it away at the end. The library allows up to 16 levels of nesting, which should be enough for almost anything.

Here's the best part: If a subroutine forgets to clean up after itself, the `destroy_lazy_context` function will destroy _all_ newer contexts along with yours. I'm not saying that's a good idea, but it's an extra safeguard.

# Usage

Simply include the header, create a context, and allocate as usual. When ready, reset the whole shebang or destroy the context.

```c
#include "lazy_alloc.h"

void some_func() {
    uint8_t lazy = create_lazy_context(20 * sizeof(char));

    char *foo = lazy_alloc(sizeof(char) * 10);
    char *bar = lazy_alloc(sizeof(char) * 10);

    reset_lazy_context();

    char *hur = lazy_alloc(sizeof(char) * 10);
    char *dur = lazy_alloc(sizeof(char) * 10);

    destroy_lazy_context(lazy);
}
```

Every new context creates a completely empty arena without affecting any existing arenas. This lets you have up to 16 levels of nesting before the library will refuse to allocate further. Even then, it's just a `define`, go ahead and change it, we're not your dad.

That's really all there is to it.

# Q & A

> Is this extremely wasteful of memory?

Yes. We preallocate arenas like a chump to save time and effort of deallocation. Wasting memory is probably just the _tip_ of the iceberg.

> Are you some kind of lazy idiot?

Yes.

> Isn't this incredibly stupid and pointless?

Yes.

> Is this faster than malloc?

Yes and no. The primary benefit to using arenas is that you don't have to free each individual allocation. The idea is to reserve a chunk of memory and then just allocate however many structs, strings, objects, or whatever you like. Then at the end, you can reset the arena to clear _all_ allocations at once and keep allocating for the next loop. This cuts the amount of memory calls roughly in half.

This is great for game engines which may allocate a whole zone of structs per event loop. Rather than allocating each of these and then freeing them every loop, just allocate them, free the whole address-space at once, and start again.

A quick test creating 1000 individual character strings in a loop 1-million times was over 2x faster than using malloc+free. Work smarter, not harder.

> Does every function need its own context?

No. The top context will continue to provide segments to any caller _until_ a new context is created. So if a function is a small/fast entity that doesn't need its own memory, it can use the parent context. Larger or more sophisticated functions should create their own.

> Why is there no `lazy_free` function?

There's no need. We're trying to break a habit here! Continuing to free every little allocation we ever make defeats the purpose.

> Is this thread safe?

Yes. The library makes use of the C-11 thread type `_Thread_local` or C-23 `thread_local` if available. These work by giving each thread its own local copy of the variable in question. In this case, each thread gets its own arena stack, so there's no need to worry about mutexes, locks, or concurrent arena access.

If you don't use threads, the library will still work just fine.

> What if my arena runs out of memory?

Make a bigger arena. The idea is to avoid allocating and deallocating. So make an arena that's large enough for the work you need to accomplish, reset it between rounds, and throw it away when you're done. Consider it a double buffer if that helps.

> What if I call a function and _it_ exhausts my arena?

Have the function create its own context, and make that context large enough for the function to do _its_ work. Routines are more likely to know their memory requirements, so this makes the most sense.

> Does this thread use linked lists?

God no.

> Why not?

Arrays are better. I refuse to elaborate further.
