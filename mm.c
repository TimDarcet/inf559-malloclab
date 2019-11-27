/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "brrrr",
    /* First member's full name */
    "Timothée Darcet",
    /* First member's email address */
    "timothee.darcet@polytechnique.edu",
    /* Second member's full name (leave blank if none) */
    "Hadrien Renaud",
    /* Second member's email address (leave blank if none) */
    "hadrien.renaud@polytechnique.edu"
};

#define DEBUG 1

typedef struct free_block {
    size_t size;
    size_t *next;
    size_t *prev;
} free_block;

/*
We use an Explicit Free Lists, using Address-ordered policy
The struct uses the structure described slide 41 of lecture 8
( available at https://moodle.polytechnique.fr/pluginfile.php/136659/mod_resource/content/6/lec8-alloc.pdf )
*/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define REAL_SIZE_FROM_USER(size) (ALIGN(size + 2*SIZE_T_SIZE))
#define GET_BLOCK_LENGTH(ptr) (*(size_t *)ptr & -2)
#define GET_PREV_TAG(ptr) ((size_t *)((char *)ptr - SIZE_T_SIZE))
#define GET_PREV_BLOCK_LENGTH(ptr) (*(GET_PREV_TAG(ptr)) & -2)
#define NEXT_BLOCK(ptr) ((void *)((char *)ptr + GET_BLOCK_LENGTH(ptr)))
#define PREV_BLOCK(ptr) ((void *)((char *)ptr - (GET_PREV_BLOCK_LENGTH(ptr))))

size_t *free_list_root = 0;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if (mem_heapsize() < 256)
    {
        mem_sbrk(ALIGN(256 - mem_heapsize()));
    }

    free_block *b = (free_block *)mem_heap_lo();
    b->next = 0;
    b->prev = 0;
    b->size = mem_heapsize();

    free_list_root = b;

    if (DEBUG)
        printf("Setting first block %x to length %d\n", (unsigned int)b, b->size);

    return 0;
}

int is_allocated(void *p)
{
    return *(size_t *)p & 1;
}

void coalesce_next(void *p)
{
    // Coalesce block pointed to by p with next block, if it is free
    size_t *n = NEXT_BLOCK(p);
    size_t *end_n = mem_heap_hi();
    if (!is_allocated(n) && n < end_n)
    {
        if (DEBUG)
            printf("coalesce merge block %x(%d) with block %x(%d)\n", (unsigned int)p, GET_BLOCK_LENGTH(p), (unsigned int)n, GET_BLOCK_LENGTH(n));

        *(size_t *)p += GET_BLOCK_LENGTH(n);
    }
}

void insert_into_list(void *p)
{
    /**
     * This function insert p into the explicit double-linked list
     * 
     * This function only makes one assumption on p: its length is 
     * the right length. It initializes prev and next to the previous
     * and next free block, if they exists, or to 0 otherwise.
     */

    free_block *b = (free_block *)p;
    size_t *start_n = mem_heap_lo();
    size_t *end_n = mem_heap_hi();
    size_t *next_free = NEXT_BLOCK(p);
    size_t *prev_free = 0;

    // We search for the next free block
    while (is_allocated(next_free) && next_free < end_n)
        next_free = NEXT_BLOCK(next_free);

    // We insert it
    if (next_free < end_n)
    {
        // Now we know that it is free
        b->next = next_free;
        prev_free = ((free_block *)next_free)->prev;
        ((free_block *)next_free)->prev = p;
    }
    else
    {
        // b is at the end of the list
        b->next = 0;

        // We search for the previous free block
        prev_free = PREV_BLOCK(p);
        while (is_allocated(prev_free) && prev_free >= start_n)
            prev_free = PREV_BLOCK(prev_free);
    }

    if (prev_free >= start_n)
    {
        b->prev = prev_free;
        ((free_block *)prev_free)->next = p;
    }
    else
    {
        // b is at the start of the list
        b->prev = 0;
        free_list_root = b;
    }
}

void coalesce_prev(void *p)
{
    size_t *previous = PREV_BLOCK(p);
    
    // previous can be equal to p if p is mem_heap_lo, in this case we don't do anything
    if (previous != p && !is_allocated(previous))
    {
        if (DEBUG)
            printf("coalesce set size for block %x to %d\n", (unsigned int)previous, GET_BLOCK_LENGTH(p) + GET_BLOCK_LENGTH(previous));
        *(size_t *)previous += GET_BLOCK_LENGTH(p);
    }
}

void coalesce(void *p)
{
    coalesce_next(p);
    coalesce_prev(p);
    insert_into_list(p);
}

void increase_heap_size(size_t size)
{
    size = ALIGN(size);
    if (DEBUG)
        printf("Increasing heapsize to %d\n", size);

    // Increase
    size_t *p = mem_sbrk(size);
    *p = size;

    if (DEBUG)
        printf("Added block at %x(%d)\n", (unsigned int)p, GET_BLOCK_LENGTH(p));

    coalesce(p);
}

void display_memory()
{
    size_t *p = mem_heap_lo();
    size_t *end_p = mem_heap_hi();

    printf("\n**** DISPLAY MEMORY ****\n");

    printf("Low at: %x, high at %x, length is %d\n", (unsigned int)p, (unsigned int)end_p, mem_heapsize());

    for (; p < end_p; p = NEXT_BLOCK(p))
    {
        if (GET_BLOCK_LENGTH(p) == 0) {
            fprintf(stderr, "Empy block found at %x, stopping display\n", (unsigned int)p);
            break;
        }
        if (is_allocated(p))
            printf("Block at %x:     allocated of size %d\n",(unsigned int) p, GET_BLOCK_LENGTH(p));
        else {
            free_block *b = (free_block *)p;
            printf("Block at %x: not allocated of size %d --> next=%x, prev=%x\n", (unsigned int)p, GET_BLOCK_LENGTH(p), (unsigned int)b->next, (unsigned int)b->prev);
        }
    }
    printf("************************\n\n");
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t user_size)
{
    size_t newsize = REAL_SIZE_FROM_USER(user_size);
    size_t tag = newsize | 1; // the block is allocated

    if (DEBUG)
    {
        display_memory();
        printf("User want to malloc %d...\n", user_size);
    }

    size_t *p = free_list_root;
    size_t *end_p = mem_heap_hi();
    while ((p < end_p) && (GET_BLOCK_LENGTH(p) <= newsize))
    {
        if (DEBUG)
            printf("Seeing block %x with length %d (allocated: %d)\n", (unsigned int)p, GET_BLOCK_LENGTH(p), is_allocated(p));
        if (GET_BLOCK_LENGTH(p) == 0) {
            fprintf(stderr, "Empy block found at %x, exiting\n", (unsigned int)p);
            display_memory();
            fprintf(stderr, "Empy block found at %x, exiting\n", (unsigned int)p);
            exit(1);
        }
        
        p = ((free_block *)p)->next; // goto next block (word addressed)
    }

    if (newsize > GET_BLOCK_LENGTH(p) || p >= end_p)
    {
        increase_heap_size(2 * newsize);
        return mm_malloc(user_size);
    }

    if (p == (void *)-1)
        return NULL;
    else
    {
        size_t old_size = GET_BLOCK_LENGTH(p);
        *(size_t *)p = tag;

        if (old_size != newsize)
        {
            size_t *next_p = NEXT_BLOCK(p);
            // Set size to the rest of the block, and leave it unallocated
            *(size_t *)next_p = ALIGN(old_size - newsize);
        }

        if (DEBUG)
            printf("Malloc %d to %x\n\n", user_size, (unsigned int)p);

        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 
 */
void mm_free(void *ptr)
{
    if (DEBUG)
        display_memory();

    size_t *p = GET_PREV_TAG(ptr);
    *p = *p & -2;

    int len_p = GET_BLOCK_LENGTH(p);

    coalesce(p);

    if (DEBUG)
    {
        printf("Freeing %d at %x. The result:\n", len_p, (unsigned int)p);
        display_memory();
    }
}

/*
 * mm_realloc - realloc using implicit simply linked list.
 * Use the free space at old pointer if possible, else do a new malloc/copy/free
 * u_* variables denote things that are as seen by the user:
 *    sizes are payload sizes
 *    pointer are pyload start pointers
 * variables with no 'u_' prefix denote things that are as they are in memory:
 *    sizes are actual block sizes
 *    pointers are block start pointers
 * *_p variables are pointers to blocks
 * *_size variables are size variables
 */
void *mm_realloc(void *ptr, size_t size)
{

    if (DEBUG)
        display_memory();

    size_t u_new_size = size;
    size_t new_size = REAL_SIZE_FROM_USER(size);
    void *u_old_p = ptr;
    void *old_p = GET_PREV_TAG(ptr);
    size_t old_size = GET_BLOCK_LENGTH(old_p);

    if (DEBUG)
        printf("User want to realloc %x(%d) to size %d\n", (unsigned int)old_p, old_size, new_size);

    if (old_size == new_size)
        return ptr;
    if (old_size == 0)
        return mm_malloc(size);

    // If there is enough space in the old block, just create a new free block after it
    if (old_size >= new_size) {
        if (DEBUG)  
            printf("The old block is large enough\n");

        // Set the old block (it is allocated)
        *(size_t *)old_p = new_size | 1;

        // 'newnext' variables correspond to the free block which has just been created
        void *newnext_p = NEXT_BLOCK(old_p);
        size_t newnext_size = old_size - new_size;
        // The 'allocated' bit is purposefully not set
        *(size_t *)newnext_p = newnext_size;
        coalesce_next(newnext_p);
        return u_old_p;
    }
    void *oldnext_p = NEXT_BLOCK(old_p);
    size_t oldnext_size = GET_BLOCK_LENGTH(oldnext_p);
    
    // If there is enough space in the old block + the next free block, use it 
    if (!is_allocated(oldnext_p) && oldnext_size + old_size >= new_size) {
        *(size_t *)old_p = new_size | 1; // It is allocated
        // Get new next block
        void *newnext_p = NEXT_BLOCK(old_p);
        // Compute its size
        size_t newnext_size = old_size + oldnext_size - new_size;
        // Set its value (it is unallocated)
        *(size_t *)newnext_p = newnext_size;
        return u_old_p;
    }

    // There is not enough space, malloc a new block, copy between the two and free the old block
    void *u_new_p = mm_malloc(u_new_size);
    if (u_new_p == NULL)
        return NULL;
    size_t u_old_size = old_size - SIZE_T_SIZE;

    // Set the copy_size to min(u_new_size, u_old_size)
    size_t u_copy_size = u_new_size < u_new_size ? u_new_size : u_old_size;
    // Copy copy_size bytes from old_ptr to new_ptr
    memcpy(u_new_p, u_old_p, u_copy_size);
    // Free the old ptr
    mm_free(u_old_p);

    return u_new_p;
}
