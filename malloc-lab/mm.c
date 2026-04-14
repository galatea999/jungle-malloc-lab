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

static void *mm_coalesce(void *ptr);
void *mm_findfit(size_t size);
void mm_place(size_t size, void *ptr);
static void * extend_heap(size_t words);

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Jungler",
    /* First member's full name */
    "Jeongwhan Kim",
    /* First member's email address */
    "dwin351@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) 
// +7은 무조건 다음 눈금에 닿게 밀어주는 역할,
// & ~0x7은 하위 3비트를 무조건 날려서 8의 배수로 만듦. 
// 앞쪽으로 밀어주고 다시 당기는 느낌
// 8을 더하면, 8을 더했을 때 internal fragmentation이 일어나버림

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // size_t : zmr
// == ALIGN(8) 헤더의 크기. ???이걸 왜 굳이 ALIGN을 해야하는가? 어차피 8인데?
// !!! 맞음. 안 해도 됨. 32bit에서 size_t가 4바이트일 떄를 대비한 방어적인 코드
// ??? 하지만, 사실 어차피 아래서 align을 또 하니까 그럴 필요 없지 않나?
/// !!! 맞음. 완전히 불필요한 코드


#define WSIZE 4 //wrd
#define DSIZE 8 //double word
#define CHUNKSIZE (1<<12) //힙을 확장할 때 한번에 늘리는 크기. 1<<12는 비트연산 =  2^12 = 4KB씩 가져오게 함
#define PACK(size, alloc) ((size) | (alloc)) //헤더에 넣을 값. allocated는 0아니면 1일 것. 그걸 OR 연산 때리면 내가 생각하는 그림이 나옴
#define GET(p)( *(unsigned int *)p)  //p가 가리키는 주소의 값을 읽어오는 매크로. 4바이트이므로 unsigned int를 씀
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 주소에 val을 쓰는 매크로. p는 void pointer일텐데, void는 타입 정보가 없어 *(dereference)가 안됨 (몇 바이트를 써야하는지 모르기에) => unsigned int *로 캐스팅이 필요함. 
#define GET_SIZE(p) (GET(p) & ~0x1) //allo 제외한 비트
#define GET_ALLOC(p) (GET(p) & 0x1) // allo만
#define HDRP(bp) ((char *)(bp)- WSIZE) //Header Pointer의 줄임말. block pointer, 즉 payload의 시작주소를 받아 헤더의 주소를 반환하는 매크로. 1바이트 단위로 계산할 것이니 char pointer를 씀.
#define FTRP(bp) ((char *)bp + (GET_SIZE(HDRP(bp)) - DSIZE)) //Footer Pointer를 찾는 코드. 
#define NEXT_BLKP(bp) ((char *)bp + (GET_SIZE(HDRP(bp)))) // 다음 블록의 payload 주소를 반환 BLKP : Block Pointer. payload의 시작주소를 말함.
#define PREV_BLKP(bp) ((char *)bp - GET_SIZE((char *)bp - DSIZE))  //이전 블록의 Payload주소를 반환

static char *heap_listp; //heap 실제로 탐색을 시작할 위치
/*
 * mm_init - initialize the malloc package.
 * ??? 왜 prologue, epilogue가 필요한가? !!! coalesce 할대, 앞뒤블록을 확인해야하는데, 힙의 맨 처음이나 끝이면 블록이 없어서 segfault가 남. 이걸 막기 위해 항상 allocated 상태인 경계 블록을 세워 두는 것.
 * ???근데 왜 epilgoue는 footer만? !!! 이전 블록 footer를 읽어야 하니, prologue에 footer가 필요한거고, epilogue는 항상 다음이니 header만.
 * ??? 그러면 prologue는 footer만 있으면 되지 않나?
 */
int mm_init(void) //heap은 [padding 4Byte][Prologue header][Prologue footer][epilogue header] 이렇게 생겨야 함.
{
    void *p = mem_sbrk(WSIZE * 4);
    if (p == (void *)-1) //실패처리
    return -1;

    PUT((char *) p, PACK(0, 0)); // 크기가 0이라는건, payload가 없는 블록이란 소리임
    PUT((char *) p+WSIZE, PACK(8, 1));
    PUT((char *) p+DSIZE, PACK(8, 1));
    PUT((char *) p+WSIZE*3, PACK(0, 1));
    
    heap_listp = (char *)p + DSIZE; //prologue footer의 위치. 여기서부터 넣을 곳 탐색이 시작될테니
    
    void * result = extend_heap(CHUNKSIZE/WSIZE); //mem_sbrk를 이용하는데, brk는 항상 epilogue를 가리키고 있으므로 그 위치에서 지가 알아서 함.
    if (result == NULL)
    return -1;

    // C에서 함수를 짤 때는 input, output의 자료형을 잘 맞춰주는게 매우 중요함.

    
    return 0;
}

// 힙을 확장하는 함수. 1) mm_init에서 - 처음에 초기 free 블록을 만들 때, 2) mm_malloc에서 - 힙을 뒤져봤는데 맞는 free 블록이 없을 때
// mem_sbrk로 힙을 늘리고, 새로 생긴 공간을 free 블록으로 세팅해서 반환 
static void * extend_heap(size_t words) //void *는 그냥 반환 타입. 새로 만든 free 블록의 주소를 반환하니 void *
{
    size_t size = ALIGN(words * WSIZE); // 지금 words 단위로 받았으므로 WSIZE를 곱해서 byte 단위로 변환
    void *bp = mem_sbrk(size); 
    if (bp == (void *)-1) // 실패 처리
    return NULL;

    //새로 확장된 공간에 header랑 footer를 줆으로써 free블록으로 만들어야 함.
    //bp를 이용하면 될듯. header를 만들때는 PACK을 이용하고, PUT으로 넣음

    //기존 Epilogue를 덮어 씀
    PUT(HDRP(bp), PACK(size, 0));

    //Prologue footer를 만들기
    PUT(FTRP(bp), PACK(size, 0));

    // 새로운 epilogue를 만듦. NEXT_BLKP는 힙 밖을 가리키지만, dereference를 하지만 않으면 segfault는 안 남.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 여기가 좀 직관적이지는 않으나, 기존에 만든 매크로를 최대한 활용해서 계산을 한 것. 품이 남는다면 코드들을 좀 더 인간에게 직관적으로 수정해봐도 좋을듯 

    return(mm_coalesce(bp));



    


}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
/* 
* 1. align the size
* 2. search a free block using mm_findfit 
* 3. if you find call mm_place
* 4. if not, extend_heap and mm_place
*/
    size = ALIGN(size + DSIZE);
    void * result = mm_findfit(size);
    if (result == NULL) {
        result = extend_heap(size/WSIZE);
        mm_place(size, result);
    }
    else {
        mm_place(size, result);
    }
    return result;

    
}

static void * mm_coalesce(void *ptr) //payload를 가리키고 있는 bp를 받을거임
{
/* Case 1 - I'm free alone 

Case 2 - We're both Free

Case 3 - Left thing is free

Case 4 - Right thing is free

*/

// CASE 1
    if (GET_ALLOC((FTRP(PREV_BLKP(ptr)))) && GET_ALLOC((HDRP(NEXT_BLKP(ptr))))) 
    {
        //make Header
        PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));

        //make Footer
        PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));

        return ptr;
    }
// CASE 2 : when both are free
    else if (GET_ALLOC((FTRP(PREV_BLKP(ptr)))) == 0 && GET_ALLOC((HDRP(NEXT_BLKP(ptr)))) == 0) 
    {

        //양 쪽 다 맞으면 헤더는 왼쪽꺼의 헤더, 푸터는 오른쪽꺼의 푸터. 사이즈가 문제인데. GET SIZE 해서 다 더해서 처리 해야할듯 
        //make Header

        size_t newsize = GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr)))+GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(newsize, 0));

        //make Footer
        PUT(FTRP(PREV_BLKP(ptr)), PACK(newsize, 0));

        return PREV_BLKP(ptr);
    }

// CASE 3 : when Left is free
    else if (GET_ALLOC((FTRP(PREV_BLKP(ptr)))) == 0 && GET_ALLOC((HDRP(NEXT_BLKP(ptr)))) == 1) 
    {
        size_t newsize = GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(PREV_BLKP(ptr)));

        //Header를 당기고, Footer는 그대로 
        PUT(HDRP(PREV_BLKP(ptr)), PACK(newsize, 0));
        
        PUT(FTRP(ptr), PACK(newsize, 0));

        return PREV_BLKP(ptr);



    }


// CASE 4 : when Right is free
    else
    {
        size_t newsize = GET_SIZE(HDRP(ptr))+ GET_SIZE(FTRP(NEXT_BLKP(ptr)));

        //Head는 그대로, Footer는 저 멀리 
        PUT(HDRP(ptr), PACK(newsize, 0));

        PUT(FTRP(ptr), PACK(newsize, 0));
    
        return ptr;
    }



}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    //header와 footer를 free로 표시
    PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
    PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));

    //coalesce 호출
    mm_coalesce(ptr);
}


//find_fit (first fit) : go round whole and fine block that is on size or above 
void * mm_findfit (size_t size)
{
    void * bp = heap_listp;
    while(GET_SIZE(HDRP(bp)) != 0)
    //go round header and compare size 
    {
        if (size <= GET_SIZE(HDRP(bp)) && GET_ALLOC(HDRP(bp)) == 0) //heap_list is initally at prologue footer
        {
            return bp;
        }
        else 
        bp = NEXT_BLKP(bp);

    }
    return NULL;

}

/*
 * place : imprint 'allocated' to free block, and if space is too big, split it. 

 You need to focus on what is input and output when you deal with function.

*/

void mm_place(size_t size, void * ptr)
{
    
    size_t newsize = GET_SIZE(HDRP(ptr)) - size;
    // if space is enough, split it. standard is DSIZE * 2 (16byte)
    if (newsize >= 16){
        
        //make Header and footer for the block we will use
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));

        // make Header and footer for splited block
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(newsize, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(newsize, 0));


    }
    // change status to allocated (header and footer)
    else {
    PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));
    PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));

    
    }
} 


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
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