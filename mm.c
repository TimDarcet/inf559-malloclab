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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define GET_BLOCK_LENGTH(ptr) (*(size_t *)ptr & -2)
#define NEXT_BLOCK(ptr) (void *)((char *)ptr + GET_BLOCK_LENGTH(ptr))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int *p = mem_heap_lo();

    if (mem_heapsize() < 256)
    {
        mem_sbrk(ALIGN(256 - mem_heapsize()));
    }

    *(size_t *)p = mem_heapsize();

    return 0;
}

void increase_heap_size()
{
    int *p = mem_heap_lo();
    int *end = mem_heap_hi();
    int used_space = 0;

    printf("Increasing heapsize to %d\n", 2 * mem_heapsize());

    // Go to end
    while (p < end)
    {
        used_space += GET_BLOCK_LENGTH(p);
        p = NEXT_BLOCK(p); // goto next block (word addressed)
    }

    // Increase
    mem_sbrk(mem_heapsize());

    *p = mem_heapsize() - used_space;
}

int is_allocated(void *p)
{
    return *(size_t *)p & 1;
}

void coalesce_next(void *p)
{
    // Coalesce block pointed to by p with next block, if it is free
    int *n = NEXT_BLOCK(p);
    if (!is_allocated(n))
    {
        *(size_t *)p += GET_BLOCK_LENGTH(n);
    }
}

void coalesce_prev(void *p)
{
    int *previous;
    for (previous = mem_heap_lo(); NEXT_BLOCK(previous) < p; previous = NEXT_BLOCK(previous));

    if (previous != p && !is_allocated(previous))
        *(size_t *)p += GET_BLOCK_LENGTH(p);
}

void coalesce(void *p){
    coalesce_prev(p);
    coalesce_next(p);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize = ALIGN(size + SIZE_T_SIZE);
    size_t tag = newsize | 1; // the block is allocated

    int *p = mem_heap_lo();
    int *end = mem_heap_hi();
    while ((p < end) && (is_allocated(p) || (GET_BLOCK_LENGTH(p) <= newsize)))
        p = NEXT_BLOCK(p);     // goto next block (word addressed)

    if (p + newsize >= end)
    {
        increase_heap_size();
        return mm_malloc(size);
    }

    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = tag;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 
 */
void mm_free(void *ptr)
{
    int *p = (int *)ptr;
    *p = *p & -2;

    coalesce(p);
}

/*
 * mm_realloc - 
 */
void *mm_realloc(void *ptr, size_t size)
{
    // usr_* variables denote things that are as seen by the user:
    //   sizes are payload sizes
    //   pointer are pyload start pointers
    // true_* variables denote things that are as they are in memory:
    //   sizes are actual block sizes
    //   pointers are block start pointers
    size_t usr_new_size = size; // usr_size is the size as seen by the usr of the mm.c functions
    size_t true_new_size = ALIGN(size + SIZE_T_SIZE); // true size is the actual memory space used to store the payload
    void *usr_old_block = ptr; // usr_old_block is the adress of the old block as seen by the user, i.e. the address of the payload
    void *true_old_block = (char *)ptr - SIZE_T_SIZE; // true_old_block is the address at which the old block actually starts, i.e. the address of the boundary tag
    size_t true_old_size = GET_BLOCK_LENGTH(true_old_block);
    if (true_old_size >= true_new_size) {
        // There is enough space in the old block
        *(size_t *)true_old_block = true_new_size;
        void *true_newnext_block = NEXT_BLOCK(true_old_block);  // 'newfree' variables correspond to the free block which has just been created
        size_t true_newnext_size = true_old_size - true_new_size;
        *(size_t *)true_newnext_block = true_newnext_size;  // The 'allocated' bit is purposefully not set
        coalesce_next(true_newnext_block);
        return usr_old_block;
    }
    void *true_oldnext_block = NEXT_BLOCK(true_old_block);
    size_t true_oldnext_size = GET_BLOCK_LENGTH(true_oldnext_block);
    if (!(*(size_t *)true_oldnext_block & 1) && true_oldnext_size + true_old_size >= true_new_size) {
        // There is enough space in the old block + the next free block 
        *(size_t *)true_old_block = true_new_size;
        void *true_newnext_block = NEXT_BLOCK(true_old_block);  // 'newfree' variables correspond to the free block which has just been created
        size_t true_newnext_size = true_old_size + true_oldnext_size - true_new_size;
        *(size_t *)true_newnext_block = true_newnext_size;  // The 'allocated' bit is purposefully not set
        return usr_old_block;
    }
    // There is not enough space, malloc a new block, copy between the two and free the old block
    void *usr_new_block = mm_malloc(usr_new_size);
    if (usr_new_block == NULL)
        return NULL;
    size_t usr_old_size = true_old_size - SIZE_T_SIZE;
    size_t usr_copy_size;
    if (usr_new_size < usr_new_size)
        usr_copy_size = usr_new_size;
    else
        usr_copy_size = usr_old_size;
    memcpy(usr_new_block, usr_old_block, usr_copy_size);
    mm_free(usr_old_block);
    return usr_new_block;
}
