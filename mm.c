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

void coalesce(void *p)
{
    int *n = NEXT_BLOCK(p);
    if (!is_allocated(n))
    {
        *(size_t *)p += GET_BLOCK_LENGTH(n);
    }
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
    while ((p < end) &&        // not passed end
           (is_allocated(p) || // already allocated
            (*p <= newsize)))  // too small
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
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    int *p = (int *)ptr;
    *p = *p & -2;

    int *n = NEXT_BLOCK(p);
    if ((*n & 1))
    {
        *p += GET_BLOCK_LENGTH(n);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *old_block = (char *)ptr - SIZE_T_SIZE;
    size_t old_size = *(size_t *)old_block;
    if (old_size >= size) {
        *(size_t *)old_block = size;

    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
