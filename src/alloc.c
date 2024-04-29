#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */
/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size)
{
    if ((block->size < size + sizeof(free_block)))
    {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *)split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block)
{
    free_block *curr = HEAD;
    while (curr != NULL)
    {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if (next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block)
{
    char *block_end = (char *)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while (curr != NULL)
    {
        if ((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block)
{
    free_block *curr = HEAD;
    if (curr == block)
    {
        HEAD = block->next;
        return;
    }
    while (curr != NULL)
    {
        if (curr->next == block)
        {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block)
{
    if (block == NULL)
    {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL)
    {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block)
        {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block)
            {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL)
    {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next)
        {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
void *do_alloc(size_t size)
{
    void *ptr = sbrk((intptr_t)(size + sizeof(header)));
    if (ptr == (void *)-1)
    {
        return NULL;
    }

    void *aligned = (void *)((intptr_t)ptr);

    header *hdr = (header *)aligned;
    hdr->size = size;
    hdr->magic = (int)0xDEADBEEF;

    return hdr + 1;
}

/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size)
{
    if (HEAD == NULL)
    {
        void *ptr = do_alloc(size);
        return ptr;
    }
    else
    {
        free_block *curr = HEAD;
        while (curr != NULL)
        {
            if (curr->size >= size + sizeof(header))
            {
                void *ptr = split(curr, (int)(size + sizeof(header)));
                if (ptr == NULL)
                {
                    return NULL;
                }
                HEAD = curr;

                remove_free_block(ptr);

                header *hdr = (header *)ptr;
                hdr->size = size;
                hdr->magic = (int)0xDEADBEEF;

                // Returns the address immediately after the header
                return hdr + 1;
            }
            curr = curr->next;
        }
        // Could not find big enough slot, ask for more.
        void *ptr = do_alloc(size);
        return ptr;
    }
}

/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size)
{
    size_t total_size = num * size;
    void *ptr = tumalloc(total_size);
    if (ptr != NULL)
    {
        memset(ptr, 0, total_size); // Initialize memory to zeros
    }
    return ptr;
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size)
{
    // Allocate new memory block
    void *new_ptr = tumalloc(new_size);
    if (new_ptr != NULL)
    {
        // Copy data from old pointer to new pointer

        header *hdr = (header *)ptr - 1;
        size_t copy_size = hdr->size < new_size ? hdr->size : new_size;
        memcpy(new_ptr, ptr, copy_size);

        // Free old memory block

        tufree(ptr);
    }

    return new_ptr;
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */

void tufree(void *ptr)
{
    // Cast ptr to header pointer and move back by 1 to get the header

    header *hdr = (header *)ptr - 1;

    // Check if magic number matches
    if (hdr->magic == 0xDEADBEEF)
    {

        // Cast header pointer to free_block pointer
        free_block *block = (free_block *)hdr;

        // Set free block size
        block->size = hdr->size;

        // Set next of free block to current HEAD
        block->next = HEAD;

        // Update HEAD to point to this free block
        HEAD = block;

        // Coalesce neighboring free blocks
        coalesce(block);
    }
    else
    {
        // Print error message and abort if magic number doesn't match
        printf("MEMORY CORRUPTION DETECTED\n");
        abort();
    }
}

