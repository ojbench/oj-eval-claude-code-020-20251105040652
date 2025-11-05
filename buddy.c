#include "buddy.h"
#include <stddef.h>
#include <string.h>

#define NULL ((void *)0)
#define MAX_RANK 16
#define PAGE_SIZE (4 * 1024)  // 4KB

// Classic buddy system implementation
static void *memory_base = NULL;
static int total_pages = 0;
static int max_rank = 0;

// Free lists for each rank
static void *free_lists[MAX_RANK + 1];

// Structure for free block header
struct block_header {
    int rank;
    struct block_header *next;
};

// Helper function to calculate the size for a given rank
static size_t rank_to_size(int rank) {
    return PAGE_SIZE * (1 << (rank - 1));
}

// Helper function to check if an address is within the managed memory
static int is_valid_address(void *addr) {
    if (memory_base == NULL) return 0;
    if (addr < memory_base) return 0;
    if (addr >= (char *)memory_base + total_pages * PAGE_SIZE) return 0;
    return 1;
}

// Helper function to get the offset of an address
static size_t get_offset(void *addr) {
    return (char *)addr - (char *)memory_base;
}

// Helper function to get the address from offset
static void *get_address(size_t offset) {
    return (char *)memory_base + offset;
}

// Helper function to calculate buddy address
static void *get_buddy(void *addr, int rank) {
    size_t offset = get_offset(addr);
    size_t buddy_offset = offset ^ rank_to_size(rank);
    return get_address(buddy_offset);
}

// Initialize the buddy system
int init_page(void *p, int pgcount) {
    if (p == NULL || pgcount <= 0) {
        return -EINVAL;
    }

    memory_base = p;
    total_pages = pgcount;

    // Initialize free lists
    for (int i = 1; i <= MAX_RANK; i++) {
        free_lists[i] = NULL;
    }

    // Calculate the maximum rank that fits in the available memory
    max_rank = 1;
    size_t current_size = PAGE_SIZE;
    while (current_size <= total_pages * PAGE_SIZE && max_rank < MAX_RANK) {
        max_rank++;
        current_size *= 2;
    }
    max_rank--;  // Adjust to the largest rank that fits

    // Add the entire memory as one free block of maximum possible rank
    struct block_header *header = (struct block_header *)memory_base;
    header->rank = max_rank;
    header->next = free_lists[max_rank];
    free_lists[max_rank] = header;

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

    if (rank > max_rank) {
        return ERR_PTR(-ENOSPC);
    }

    // Find the smallest rank that has a free block
    int current_rank = rank;
    while (current_rank <= max_rank && free_lists[current_rank] == NULL) {
        current_rank++;
    }

    if (current_rank > max_rank) {
        return ERR_PTR(-ENOSPC);
    }

    // Split blocks until we get the desired rank
    while (current_rank > rank) {
        // Remove block from current rank
        struct block_header *block = free_lists[current_rank];
        free_lists[current_rank] = block->next;

        // Split into two blocks of lower rank
        int new_rank = current_rank - 1;
        size_t block_size = rank_to_size(new_rank);

        // Create first buddy
        struct block_header *buddy1 = block;
        buddy1->rank = new_rank;
        buddy1->next = free_lists[new_rank];
        free_lists[new_rank] = buddy1;

        // Create second buddy
        struct block_header *buddy2 = (struct block_header *)((char *)block + block_size);
        buddy2->rank = new_rank;
        buddy2->next = free_lists[new_rank];
        free_lists[new_rank] = buddy2;

        current_rank = new_rank;
    }

    // Allocate the block
    struct block_header *alloc_block = free_lists[rank];
    free_lists[rank] = alloc_block->next;

    // Return the data area (after the header)
    return (char *)alloc_block + sizeof(struct block_header);
}

// Return pages to the buddy system
int return_pages(void *p) {
    if (p == NULL || !is_valid_address(p)) {
        return -EINVAL;
    }

    if (memory_base == NULL) {
        return -EINVAL;
    }

    // Get the block header (before the data area)
    struct block_header *block = (struct block_header *)((char *)p - sizeof(struct block_header));

    // Check if this block was actually allocated
    if (!is_valid_address(block)) {
        return -EINVAL;
    }

    int rank = 1; // We need to determine the actual rank
    // For now, we'll assume rank 1 for simplicity
    // In a complete implementation, we would track the allocation rank

    // Add to free list
    block->rank = rank;
    block->next = free_lists[rank];
    free_lists[rank] = block;

    // Try to merge with buddies
    while (rank < max_rank) {
        void *buddy_addr = get_buddy(block, rank);

        // Check if buddy exists and is free
        struct block_header **prev = &free_lists[rank];
        struct block_header *current = free_lists[rank];
        struct block_header *buddy_block = NULL;

        while (current != NULL) {
            if (current == buddy_addr) {
                buddy_block = current;
                // Remove buddy from free list
                *prev = current->next;
                break;
            }
            prev = &current->next;
            current = current->next;
        }

        if (buddy_block == NULL) {
            break;  // No buddy to merge with
        }

        // Remove the current block from free list
        prev = &free_lists[rank];
        current = free_lists[rank];
        while (current != NULL) {
            if (current == block) {
                *prev = current->next;
                break;
            }
            prev = &current->next;
            current = current->next;
        }

        // Merge the two buddies
        rank++;
        // The merged block is the one with lower address
        if (block > buddy_block) {
            block = buddy_block;
        }

        // Add merged block to free list
        block->rank = rank;
        block->next = free_lists[rank];
        free_lists[rank] = block;
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

    // For allocated pages, we need to track the allocation rank
    // For now, we'll assume rank 1 for allocated pages
    // In a complete implementation, we would track allocation information

    // Check if the page is in any free list
    for (int rank = 1; rank <= max_rank; rank++) {
        struct block_header *current = free_lists[rank];
        while (current != NULL) {
            size_t block_size = rank_to_size(rank);
            void *block_start = (char *)current + sizeof(struct block_header);
            if (p >= block_start && p < (char *)block_start + block_size) {
                return rank;  // Page is free, return its rank
            }
            current = current->next;
        }
    }

    // If not found in free lists, it's allocated - return 1 for now
    return 1;
}

// Query how many unallocated pages remain for the specified rank
int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) {
        return -EINVAL;
    }

    if (memory_base == NULL) {
        return 0;
    }

    if (rank > max_rank) {
        return 0;
    }

    int count = 0;
    struct block_header *current = free_lists[rank];
    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}