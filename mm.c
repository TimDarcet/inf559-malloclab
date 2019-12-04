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

#define DEBUG

typedef struct free_block {
    size_t size; // MEF: size holds the size + the bit field used to mark allocation. For safety, access this field using GET_BLOCK_LENGTH when reading
    struct free_block *next;
    struct free_block *prev;
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

size_t *free_list_root = NULL;

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
    b->next = NULL;
    b->prev = NULL;
    b->size = mem_heapsize();
    *GET_PREV_TAG(NEXT_BLOCK(b)) = b->size;

    free_list_root = (size_t *)b;

    #ifdef DEBUG
    printf("Setting first block %p to length %d\n", b, b->size);
    #endif
    return 0;
}

//TODO: this could be a macro
int is_allocated(void *p)
{
    return (*(size_t *)p) & 1;
}

/*
 * This function insert p into the explicit double-linked list
 * 
 * This function only makes one assumption on p: its length is 
 * the right length. It initializes prev and next to the previous
 * and next free block, if they exists, or to 0 otherwise.
 */
void insert_into_list(void *p)
{
    free_block *b = (free_block *)p;
    size_t *start_n = mem_heap_lo();
    size_t *end_n = mem_heap_hi();
    size_t *next_free;
    size_t *prev_free = NULL;

    // We search for the next free block
    for (next_free = NEXT_BLOCK(p);
         next_free < end_n && is_allocated(next_free);
         next_free = NEXT_BLOCK(next_free));

    // We insert it
    if (next_free < end_n) {
        // Now we know that it is free
        b->next = (free_block *)next_free;
        prev_free = (size_t *)((free_block *)next_free)->prev;
        ((free_block *)next_free)->prev = (free_block *)p;
    }
    else {
        // b is at the end of the list
        b->next = NULL;

        // We search for the previous free block
        prev_free = p;
        do {
            if (prev_free == start_n) {
                prev_free = NULL;
                break;
            }
            prev_free = PREV_BLOCK(prev_free);
        } while (is_allocated(prev_free));

    }

    if (prev_free >= start_n) {
        b->prev = (free_block *)prev_free;
        ((free_block *)prev_free)->next = (free_block *)p;
    }
    else {
        // b is at the start of the list
        b->prev = NULL;
        free_list_root = (size_t *)b;
    }
}


int coalesce_next(free_block *p)
{
    #ifdef DEBUG
    if (is_allocated(p)) {
        printf("Tried to coalesce allocated block. Exiting.");
        exit(1);
    }
    #endif
    // Coalesce block pointed to by p with next block, if it is free
    free_block *n = NEXT_BLOCK(p);
    free_block *end_n = mem_heap_hi();
    if (n < end_n && !is_allocated(n))
    {
        #ifdef DEBUG
            printf("coalesce %p (size=%d) with %p(size=%d)\n", p, GET_BLOCK_LENGTH(p), n, GET_BLOCK_LENGTH(n));
        #endif
        p->size += n->size; // n->size works because the block is free
        *GET_PREV_TAG(NEXT_BLOCK(p)) = p->size;
        p->next = n->next;
        p->next->prev = p;
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Coalesce the block with the previous block if it is free
 * Returns 1 if it coalesced, 0 elsewise
 */
int coalesce_prev(free_block *p)
{
    #ifdef DEBUG
    if (is_allocated(p)) {
        printf("Tried to coalesce allocated block. Exiting.");
        exit(1);
    }
    #endif
    // Handle this case as PREV_BLOCK is not defined there
    if (p == mem_heap_lo()) {
        #ifdef DEBUG
        printf("%p is mem_heap_lo()\n", p);
        #endif
        return 0;
    }

    free_block *previous = PREV_BLOCK(p);
    
    printf("Previous is %p of size %d\n", previous, GET_BLOCK_LENGTH(previous));
    if (!is_allocated(previous)) {
        #ifdef DEBUG
            printf("coalesce %p (size=%d) with %p (size=%d)", p, GET_BLOCK_LENGTH(p), previous, GET_BLOCK_LENGTH(p) + GET_BLOCK_LENGTH(previous));
        #endif
        previous->size += GET_BLOCK_LENGTH(p);
        *GET_PREV_TAG(NEXT_BLOCK(p)) = previous->size;
        previous->next = p->next;
        previous->next->prev = previous;
        return 1;
    }
    else {
        #ifdef DEBUG
        printf("Prev is allocated, do not coalesce\n", p);
        #endif
        return 0;
    }
}

/*
 * Coalesce block at p on both sides
 * Suppose free block is already nicely in the free list
 */
void coalesce(void *p)
{
    #ifdef DEBUG
        printf("Coalesce %p with nexts\n", p);
    #endif
    while(coalesce_next(p));
    #ifdef DEBUG
        printf("Coalesce %p with prevs\n", p);
    #endif
    while(coalesce_prev(p))
        p = PREV_BLOCK(p);
    #ifdef DEBUG
        printf("Coalesced\n");
    #endif
    
}

void increase_heap_size(size_t size)
{
    size = ALIGN(size);
    #ifdef DEBUG
        printf("Increasing heapsize by %d\n", size);
    #endif
    // Increase
    free_block *p = mem_sbrk(size);
    p->size = size;
    *GET_PREV_TAG(NEXT_BLOCK(p)) = p->size; 

    #ifdef DEBUG
        printf("Added block at %p(%d)\n", p, GET_BLOCK_LENGTH(p));
    #endif
    //TODO: This could be optimized using the fact that we insert at the end of the heap
    insert_into_list(p);
    coalesce(p);
}

void display_memory()
{
    size_t *p = mem_heap_lo();
    size_t *end_p = mem_heap_hi();

    printf("\n************************\n**** DISPLAY MEMORY ****\n");

    printf("Low at: %p, high at %p, length is %d\n", p, end_p, mem_heapsize());

    for (; p < end_p; p = NEXT_BLOCK(p))
    {
        if (GET_BLOCK_LENGTH(p) == 0) {
            fprintf(stderr, "Empty block found at %p, stopping display\n", p);
            break;
        }
        if (is_allocated(p)) {
            printf("Block at %p:     allocated of size %d\n", p, GET_BLOCK_LENGTH(p));
        }
        else {
            free_block *b = (free_block *)p;
            printf("Block at %p: not allocated of size %d --> next=%p, prev=%p\n", p, GET_BLOCK_LENGTH(p), b->next, b->prev);
        }
        size_t size1 = GET_BLOCK_LENGTH(p);
        size_t size2 = GET_PREV_BLOCK_LENGTH(NEXT_BLOCK(p));
        if (size1 == size2)
            printf("Sizes match!\n");
        else
            printf("Sizes do not match! size1=%d, size2=%d\n", size1, size2);
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

    #ifdef DEBUG
        display_memory();
        printf("User want to malloc %d...\n", user_size);
    #endif

    free_block *p;
    size_t *end_p = mem_heap_hi();
    for (p = (free_block *)free_list_root;
         (p < (free_block *)end_p) && (GET_BLOCK_LENGTH(p) <= newsize);
         p = p->next)
    {
        #ifdef DEBUG
            printf("Seeing block %p with length %d (allocated: %d)\n", p, GET_BLOCK_LENGTH(p), is_allocated(p));
        #endif
        if (GET_BLOCK_LENGTH(p) == 0) {
            fprintf(stderr, "Empty block found at %p, exiting\n", p);
            display_memory();
            fprintf(stderr, "Empty block found at %p, exiting\n", p);
            exit(1);
        }
        
        if (p->next == NULL)
            break; // goto next block (word addressed)
    }

    if (newsize > GET_BLOCK_LENGTH(p) || p >= (free_block *)end_p)
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
        *GET_PREV_TAG(NEXT_BLOCK(p)) = newsize;

        if (old_size != newsize)
        {
            size_t *next_p = NEXT_BLOCK(p);
            // Set size to the rest of the block, and leave it unallocated
            *(size_t *)next_p = ALIGN(old_size - newsize);
            *GET_PREV_TAG(NEXT_BLOCK(next_p)) = ALIGN(old_size - newsize);
        }

        #ifdef DEBUG
            printf("Malloc %d to %p\n\n", user_size, p);
        #endif
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 
 */
void mm_free(void *ptr)
{
    #ifdef DEBUG
        display_memory();
    #endif
    size_t *p = GET_PREV_TAG(ptr);
    *p = *p & -2;

    insert_into_list(p);
    coalesce(p);

    #ifdef DEBUG
        int len_p = GET_BLOCK_LENGTH(p);
        printf("Freeing %d at %p. The result:\n", len_p, p);
        display_memory();
    #endif
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

    #ifdef DEBUG
        display_memory();
    #endif

    size_t u_new_size = size;
    size_t new_size = REAL_SIZE_FROM_USER(size);
    void *u_old_p = ptr;
    void *old_p = GET_PREV_TAG(ptr);
    size_t old_size = GET_BLOCK_LENGTH(old_p);

    #ifdef DEBUG
        printf("User want to realloc %p(%d) to size %d\n", old_p, old_size, new_size);
    #endif

    if (old_size == new_size)
        return ptr;
    if (old_size == 0)
        return mm_malloc(size);

    // If there is enough space in the old block, just create a new free block after it
    if (old_size >= new_size) {
        #ifdef DEBUG
            printf("The old block is large enough\n");
        #endif
    
        // Set the old block (it is allocated)
        *(size_t *)old_p = new_size | 1;
        // 'newnext' variables correspond to the free block which has just been created
        void *newnext_p = NEXT_BLOCK(old_p);
        size_t newnext_size = old_size - new_size;
        // The 'allocated' bit is purposefully not set
        *(size_t *)newnext_p = newnext_size;
        // Set boundary tag
        *GET_PREV_TAG(NEXT_BLOCK(newnext_p)) = newnext_size;
        insert_into_list(newnext_p);
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
        // Set boundary tag
        *GET_PREV_TAG(NEXT_BLOCK(newnext_p)) = newnext_size;
        insert_into_list(newnext_p);
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
