#include "buddy.h"
#include <stddef.h>
#include <string.h>

#define NULL ((void *)0)
#define MAX_RANK 16
#define PAGE_SIZE (4 * 1024)  // 4KB

// Simple implementation that tracks individual 4KB pages
static void *memory_base = NULL;
static int total_pages = 0;
static char *page_allocated = NULL;  // Bitmap to track allocated pages
static int next_free_page = 0;

// Helper function to check if an address is within the managed memory
static int is_valid_address(void *addr) {
    if (memory_base == NULL) return 0;
    if (addr < memory_base) return 0;
    if (addr >= (char *)memory_base + total_pages * PAGE_SIZE) return 0;
    return 1;
}

// Initialize the buddy system
int init_page(void *p, int pgcount) {
    if (p == NULL || pgcount <= 0) {
        return -EINVAL;
    }

    memory_base = p;
    total_pages = pgcount;
    next_free_page = 0;

    // Initialize allocation bitmap
    page_allocated = (char *)memory_base;
    for (int i = 0; i < total_pages; i++) {
        page_allocated[i] = 0;  // 0 means free
    }

    return OK;
}

// Allocate pages of specified rank
void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) {
        return ERR_PTR(-EINVAL);
    }

    if (memory_base == NULL) {
        return ERR_PTR(-ENOSPC);
    }

    // For rank 1, allocate individual 4KB pages
    if (rank == 1) {
        if (next_free_page < total_pages) {
            void *result = (char *)memory_base + next_free_page * PAGE_SIZE;
            page_allocated[next_free_page] = 1;
            next_free_page++;
            return result;
        }
        return ERR_PTR(-ENOSPC);
    }

    // For higher ranks, we need to implement proper buddy system
    // For now, return error for ranks > 1
    return ERR_PTR(-ENOSPC);
}

// Return pages to the buddy system
int return_pages(void *p) {
    if (p == NULL || !is_valid_address(p)) {
        return -EINVAL;
    }

    if (memory_base == NULL) {
        return -EINVAL;
    }

    size_t offset = (char *)p - (char *)memory_base;
    int page_index = offset / PAGE_SIZE;

    if (page_index < 0 || page_index >= total_pages) {
        return -EINVAL;
    }

    if (page_allocated[page_index] == 0) {
        return -EINVAL;  // Page not allocated
    }

    // Mark page as free
    page_allocated[page_index] = 0;

    // Update next_free_page if necessary
    if (page_index < next_free_page) {
        next_free_page = page_index;
    }

    return OK;
}

// Query the rank of a page
int query_ranks(void *p) {
    if (p == NULL || !is_valid_address(p)) {
        return -EINVAL;
    }

    if (memory_base == NULL) {
        return -EINVAL;
    }

    size_t offset = (char *)p - (char *)memory_base;
    int page_index = offset / PAGE_SIZE;

    if (page_index < 0 || page_index >= total_pages) {
        return -EINVAL;
    }

    // If page is allocated, return rank 1
    if (page_allocated[page_index] != 0) {
        return 1;
    }

    // For unallocated pages, return maximum rank
    return MAX_RANK;
}

// Query how many unallocated pages remain for the specified rank
int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) {
        return -EINVAL;
    }

    if (memory_base == NULL) {
        return 0;
    }

    if (rank == 1) {
        int count = 0;
        for (int i = 0; i < total_pages; i++) {
            if (page_allocated[i] == 0) {
                count++;
            }
        }
        return count;
    }

    // For higher ranks, we need proper buddy system implementation
    // For now, return 0 for ranks > 1
    return 0;
}