// alloc.h
#pragma once
#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// allocator API
bool  mm_init(void);
void* mm_malloc(size_t bytes);
void  mm_free(void* ptr);
void  mm_dump(void);

// debug / helpers
bool mm_checkheap(int verbose);
void* mem_sbrk(intptr_t incr);
size_t mm_heap_bytes(void);
