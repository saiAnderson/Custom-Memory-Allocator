// main.c
#include "alloc.h"
#include <stdio.h>

int main(void) {
    if (!mm_init()) {
        printf("mm_init failed\n");
        return 1;
    }
    printf("\n== After init ==\n");
    mm_dump();
    void* a = mm_malloc(1);
    void* b = mm_malloc(16);
    void* c = mm_malloc(17);
    printf("\n== After malloc a(1), b(16), c(17) ==\n");
    mm_dump();

    mm_free(a);
    printf("\n== After free(a) ==\n");
    mm_dump();

    mm_free(b);
    printf("\n== After free(b) (should coalesce) ==\n");
    mm_dump();

    mm_free(c);
    printf("\n== After free(c) (should coalesce) ==\n");
    mm_dump();

    return 0;
}
