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


// #define SIMPLY_LINKED_IMPLICIT
#define DOUBLY_LINKED_IMPLICIT



/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define GET_BLOCK_LENGTH(ptr) (*(size_t *)ptr & -2)
#define GET_PREV_TAG(ptr) ((size_t *)((char *)ptr - SIZE_T_SIZE))
#define GET_PREV_BLOCK_LENGTH(ptr) (*(GET_PREV_TAG(ptr)) & -2)
#define NEXT_BLOCK(ptr) ((void *)((char *)ptr + GET_BLOCK_LENGTH(ptr)))
#define PREV_BLOCK(ptr) ((void *)((char *)ptr - (GET_PREV_BLOCK_LENGTH(ptr))))


#ifdef SIMPLY_LINKED_IMPLICIT
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

void increase_heap_size()
{
    printf("Increasing heapsize to %d\n", 2 * mem_heapsize());

    // Increase
    int *p = mem_sbrk(mem_heapsize());

    coalesce(p);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t user_size)
{
    size_t newsize = ALIGN(user_size + SIZE_T_SIZE);
    size_t tag = newsize | 1; // the block is allocated

    int *p = mem_heap_lo();
    int *end_p = mem_heap_hi();
    while ((p < end_p) && (is_allocated(p) || (GET_BLOCK_LENGTH(p) <= newsize)))
        p = NEXT_BLOCK(p);     // goto next block (word addressed)

    if (p + newsize >= end_p)
    {
        increase_heap_size();
        // TODO pas de recursif
        return mm_malloc(user_size);
    }

    if (p == (void *)-1)
        return NULL;
    else
    {
        size_t old_size = GET_BLOCK_LENGTH(p);
        *(size_t *)p = tag;

        if (old_size != newsize) {
            int *next_p = NEXT_BLOCK(p);
            *(size_t *)next_p = (old_size - newsize) | 1;
        }

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

    size_t u_new_size = size;
    size_t new_size = ALIGN(size + SIZE_T_SIZE);
    void *u_old_p = ptr;
    void *old_p = (char *)ptr - SIZE_T_SIZE;
    size_t old_size = GET_BLOCK_LENGTH(old_p);
    
    // If there is enough space in the old block, just create a new free block after it
    if (old_size >= new_size) {
        *(size_t *)old_p = new_size;
        void *newnext_p = NEXT_BLOCK(old_p);  // 'newnext' variables correspond to the free block which has just been created
        size_t newnext_size = old_size - new_size;
        *(size_t *)newnext_p = newnext_size;  // The 'allocated' bit is purposefully not set
        coalesce_next(newnext_p);
        return u_old_p;
    }
    void *oldnext_p = NEXT_BLOCK(old_p);
    size_t oldnext_size = GET_BLOCK_LENGTH(oldnext_p);
    
    // If there is enough space in the old block + the next free block, use it 
    if (!(*(size_t *)oldnext_p & 1) && oldnext_size + old_size >= new_size) {
        *(size_t *)old_p = new_size;
        void *newnext_p = NEXT_BLOCK(old_p);
        size_t newnext_size = old_size + oldnext_size - new_size;
        *(size_t *)newnext_p = newnext_size;
        return u_old_p;
    }

    // There is not enough space, malloc a new block, copy between the two and free the old block
    void *u_new_p = mm_malloc(u_new_size);
    if (u_new_p == NULL)
        return NULL;
    size_t u_old_size = old_size - SIZE_T_SIZE;
    size_t u_copy_size;
    if (u_new_size < u_new_size)
        u_copy_size = u_new_size;
    else
        u_copy_size = u_old_size;
    memcpy(u_new_p, u_old_p, u_copy_size);
    mm_free(u_old_p);
    return u_new_p;
}
#endif
#ifdef DOUBLY_LINKED_IMPLICIT

/*
 * coalesce_next - Coalesce block pointed to by p with next block, if it is free
 */
void coalesce_next(void *p)
{
    void *n = NEXT_BLOCK(p);
    if (!is_allocated(n))
    {
        size_t old_size = GET_BLOCK_LENGTH(p);
        *(size_t *)p += GET_BLOCK_LENGTH(n);
        *GET_PREV_TAG(NEXT_BLOCK(p)) += old_size;
    }
}

/*
 * coalesce_prev - Coalesce block pointed to by p with previous block, if it is free
 */
void coalesce_prev(void *p)
{
    void *prev = PREV_BLOCK(p);
    if (!is_allocated(prev))
    {
        *GET_PREV_TAG(NEXT_BLOCK(p)) += GET_BLOCK_LENGTH(prev);
        *(size_t *)prev += GET_BLOCK_LENGTH(p);
    }
}

/*
 * coalesce - Coalesce block pointed to by p with next and previous block, if they are free
 */
void coalesce(void *p){
    coalesce_next(p);
    coalesce_prev(p);
}
#endif