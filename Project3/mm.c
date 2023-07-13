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
#include <stdint.h>

#include "mm.h"
#include "memlib.h"
/*Basic constants and macros*/
//Word and header/footer size (bytes)
#define WSIZE 4
//Double word size (bytes)
#define DSIZE 8
//Extend heap by this amount (bytes)
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y)? (x) : (y))

//Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

//Read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// Read the size and allocated fields from address p
// 8-byte alignment
#define GET_SIZE(p) (GET(p) & ~0x7)
// LSB is alloc/free bit
#define GET_ALLOC(p) (GET(p) & 0x1)

//Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//Given free list ptr p, compute address of next and previous freed block
#define NEXT_FP(p) (*(char **)(p + WSIZE))
#define PREV_FP(p) (*(char **)(p))
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20211606",
    /* Your full name*/
    "Seokgi Han",
    /* Your email address */
    "hski0930@sogang.ac.kr",
};

static char *free_listp;
static char *heap_listp;
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void add_to_flist(void *bp);
static void delete_from_flist(void *bp);

static void delete_from_flist(void *bp)
{
    // if bp is free_listp
    if(PREV_FP(bp) == NULL) {
        free_listp=NEXT_FP(bp);
        PREV_FP(free_listp) = NULL;
    }
    else {
        NEXT_FP(PREV_FP(bp)) = NEXT_FP(bp);
        PREV_FP(NEXT_FP(bp)) = PREV_FP(bp);
    }
}
static void add_to_flist(void *bp)
{
    // add to prev of free list head pointer
    NEXT_FP(bp) = free_listp;
    PREV_FP(free_listp) = bp;
    PREV_FP(bp)=NULL;
    // then, added free item is header
    free_listp = bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1: Already allocated both of prev and next
    if(prev_alloc && next_alloc){
        add_to_flist(bp);
        return bp;
    }
    // Case 2: prev allocated and next not allocated
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_from_flist(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // Case 3: prev not allocated and next allocated
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        delete_from_flist(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // Case 4: both of prev and next not allocated
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        delete_from_flist(PREV_BLKP(bp));
        delete_from_flist(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_to_flist(bp);
    return bp;
}
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // If enough space is left, split the list and update metadata
    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        delete_from_flist(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp);
    }
    // If left-over size is less than the minimum block size (16 bytes), ignore (internal fragmentation)
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        delete_from_flist(bp);
    }
}
static void *find_fit(size_t asize)
{
    //First-fit search
    void *bp;

    //Search in free list
    for(bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FP(bp)){
        if(GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
    }
    //No fit
    return NULL;
}
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;
    
    // Initialize free block header/footer and the epilogue header
    
    // Free block header
    PUT(HDRP(bp), PACK(size, 0));
    // Free block footer
    PUT(FTRP(bp), PACK(size, 0));
    // New epilogue header
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // Coalesce if the previous block was free
    return coalesce(bp);
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap*/
    // if 8*WSIZE is 4*WSIZE, it is segment falut
    if((heap_listp = mem_sbrk(8*WSIZE)) == (void *)-1) return -1;
    // Alignment padding
    PUT(heap_listp, 0);
    // Prologue header
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // Prologue footer
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // Epilogue header
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);
    free_listp = heap_listp;
    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //Adjusted block size
    size_t asize;
    //Amount to extend heap if no fit
    size_t extendsize;
    char *bp;

    // Ignore spurious requests
    if(size == 0) return NULL; 

    // Adjust block size to include overhead and alignment reqs
    if(size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // Search the free list for a fit
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    // No fit found, Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if(bp == NULL) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *oldptr, size_t size)
{
    size_t oldsize, newsize;
    void *newptr;

    // Because size_t is unsigned integer type..
    // Type casting
    if((int)size < 0) return NULL;
    if(size == 0){
        mm_free(oldptr);
        return NULL;
    }
    
    // If current block is NULL, malloc
    if(oldptr == NULL) return mm_malloc(size);
    else{
        oldsize = GET_SIZE(HDRP(oldptr));
        newsize = size + (2 * WSIZE);
    }

    if(newsize > oldsize){
        size_t n_is_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
        size_t n_blk_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        size_t co_fsize = oldsize + n_blk_size;
        
        if(n_is_alloc == 0 && co_fsize >= newsize){
            delete_from_flist(NEXT_BLKP(oldptr));
            PUT(HDRP(oldptr), PACK(co_fsize, 1));
            PUT(FTRP(oldptr), PACK(co_fsize, 1));
            return oldptr;
        }
        else{
            newptr=mm_malloc(newsize);
            if(newptr == NULL) return NULL;

            place(newptr, newsize);
            memcpy(newptr, oldptr, oldsize);
            mm_free(oldptr);
            return newptr;
        }
    }
    else return oldptr;
}
