#ifndef LAZY_ALLOC_H
#define LAZY_ALLOC_H 1

#include <threads.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Library memory structures
 * 
 * This library consists of:
 * 
 * 1. A definition of a very simple arena memory structure
 * 2. A stack to allow multiple co-existing arenas in reserved contexts
 * 
 * The stack itself should be considered "private", but this is a header, not
 * a library. Manipulate the stack or arenas if you really feel the need.
 */

#define MAX_LAZY_STACK 16 // 640kb aught to be enough for anyone.

typedef struct {
    void* memory;   // Arena's backing memory
    size_t size;    // Total size of the arena
    size_t offset;  // Current position
} LazyArena;

typedef struct {
    LazyArena* arenas[MAX_LAZY_STACK]; // Stack of arenas
    int top;                           // Index of top arena (-1 if empty)
} LazyArenaStack;

#if defined(__STDC_VERSION__)
#   if (__STDC_VERSION__ >= 202301L )
static thread_local LazyArenaStack lazy_stack = { .top = -1 };
#   elif (__STDC_VERSION__ >= 201101L )
static _Thread_local LazyArenaStack lazy_stack = { .top = -1 };
#   endif
#else
static LazyArenaStack lazy_stack = { .top = -1 };
#endif

/**
 * Create a memory context for all subsequent allocations
 * 
 * Create an arena of the requested size and push it onto a stack of arenas.
 * This allows multiple levels of nesting, and all allocations will always
 * target the current nesting level. This makes it possible for routines to
 * build and dispose of scratch arenas.
 */
uint8_t create_lazy_context(size_t size) {

    if (lazy_stack.top + 1 >= MAX_LAZY_STACK)
        return -1;

    // Attempt to allocate both the arena and its memory.

    LazyArena* arena = malloc(sizeof(LazyArena));
    if (arena == NULL)
        return -1;

    arena->memory = malloc(size);

    if (!arena->memory) {
        free(arena);
        return -1;
    }

    // If successful, bootstrap the arena attributes, push it onto the stack
    // of arena contexts, and return the arena identifier. The caller will 
    // need this to deallocate everything.

    arena->size = size;
    arena->offset = 0;
    lazy_stack.arenas[++lazy_stack.top] = arena;

    return lazy_stack.top;
}


/**
 * Allocate some memory similarly to malloc, and return the memory pointer
 * 
 * Set aside the amount of requested memory in the current arena and return a
 * pointer. This will require a bit of pointer math
 */
void* lazy_alloc(size_t size) {

    if (lazy_stack.top < 0)
        return NULL;

    LazyArena* arena = lazy_stack.arenas[lazy_stack.top];

    if (arena->offset + size > arena->size)
        return NULL;

    void* ptr = (char*)arena->memory + arena->offset;
    arena->offset += size;

    return ptr;
}


/**
 * Destroy the indicated context and any higher contexts
 * 
 * Using this library only means tracking a single context value. Passing that
 * value to this function will remove that arena context from our stack, thus
 * permanently cleaning up the structure.
 * 
 * If a function ends prematurely or someone was even lazier than expected and
 * didn't destroy their context before returning, this function will also prune
 * all higher contexts.
 */
void destroy_lazy_context(uint8_t context) {

    if (lazy_stack.top < 0)
        return;

    // If we were passed a context _lower_ than the current context, clean up
    // all higher contexts in addition to our own. Someone got _too_ lazy.

    while (lazy_stack.top >= context) {
        LazyArena* arena = lazy_stack.arenas[lazy_stack.top--];
        free(arena->memory);
        free(arena);
    }
}


/** 
 * Rewind the current arena stack without deallocating
 * 
 * This function saves a lot of free() work by just resetting the offset to
 * the beginning of the arena, and thus "deallocating" everything all at once.
 */
void reset_lazy_context() {
    if (lazy_stack.top >= 0)
        lazy_stack.arenas[lazy_stack.top]->offset = 0;
}


#endif
