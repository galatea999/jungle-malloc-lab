/*
 * memlib.c - 힙 메모리 시뮬레이터
 *
 * 실제 OS의 sbrk() 시스템콜 대신, 미리 할당해둔 큰 배열로
 * 힙을 흉내낸다. 덕분에 student의 malloc과 libc의 malloc이
 * 충돌하지 않고 독립적으로 테스트할 수 있다.
 *
 * 핵심 개념:
 *   실제 힙:  프로세스 데이터 세그먼트 끝(brk)을 sbrk()로 늘려서 사용
 *   이 파일:  malloc()으로 미리 큰 버퍼(MAX_HEAP)를 잡고, 그 안에서 brk를 흉내냄
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* --------------------------------------------------------
 * 힙을 표현하는 세 개의 포인터
 *
 *  mem_start_brk          mem_brk        mem_max_addr
 *       ↓                    ↓                ↓
 *  [....사용 중인 힙 영역....][....비어있는 영역....]
 *
 *  mem_sbrk(n) 을 호출하면:
 *    - mem_brk가 n만큼 오른쪽으로 이동
 *    - 이동 전 mem_brk 주소를 반환 (새로 확장된 영역의 시작점)
 * -------------------------------------------------------- */
static char *mem_start_brk;  /* 힙의 첫 번째 바이트 주소 */
static char *mem_brk;        /* 현재 힙의 끝 (여기서부터 미사용) */
static char *mem_max_addr;   /* 힙의 최대 주소 (이 이상 늘릴 수 없음) */


/* --------------------------------------------------------
 * mem_init: 힙 시뮬레이터 초기화
 *
 * MAX_HEAP 크기의 메모리를 실제 malloc으로 미리 확보한다.
 * 이 공간을 student의 malloc이 쪼개서 사용하게 된다.
 * -------------------------------------------------------- */
void mem_init(void)
{
    /* libc의 malloc으로 큰 버퍼를 미리 잡아둠 */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
        fprintf(stderr, "mem_init_vm: malloc error\n");
        exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 힙 최대 주소 */
    mem_brk = mem_start_brk;                  /* 처음엔 힙이 비어있음 */
}


/* --------------------------------------------------------
 * mem_deinit: 힙 시뮬레이터 정리
 * mem_init에서 잡은 버퍼를 해제한다.
 * -------------------------------------------------------- */
void mem_deinit(void)
{
    free(mem_start_brk);
}


/* --------------------------------------------------------
 * mem_reset_brk: 힙을 빈 상태로 리셋
 * mem_brk를 다시 mem_start_brk로 되돌린다.
 * -------------------------------------------------------- */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}


/* --------------------------------------------------------
 * mem_sbrk: 힙을 incr 바이트만큼 늘린다
 *
 * 실제 sbrk()와 동일한 인터페이스:
 *   - 성공: 확장 전 mem_brk 주소 반환 (새로 생긴 영역의 시작)
 *   - 실패: (void*)-1 반환
 *
 * 예시:
 *   mem_brk = 1000 일 때 mem_sbrk(32) 호출하면
 *   → mem_brk = 1032 가 되고
 *   → 1000 을 반환  ← mm_malloc에서 p로 받는 값이 이것
 *
 * 주의: 이 구현에서는 힙을 줄이는 것(incr < 0)은 허용하지 않는다.
 * -------------------------------------------------------- */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;  /* 현재 brk를 저장해두고. 왜? 성공해서 반환할 때 쓰기 위해 */

    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1;
    }

    mem_brk += incr;    /* brk를 incr만큼 앞으로 민다 */
    return (void *)old_brk;  /* 확장 전 주소를 반환 */
}


/* --------------------------------------------------------
 * 아래는 힙 상태를 조회하는 유틸리티 함수들
 * -------------------------------------------------------- */

/* 힙의 첫 번째 바이트 주소 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 힙의 마지막 바이트 주소 (mem_brk - 1) */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/* 현재 사용 중인 힙의 총 크기 (바이트) */
size_t mem_heapsize()
{
    return (size_t)(mem_brk - mem_start_brk); //왜 -1? !!! mem_brk는 사실 힙 바깥임. 여기서부터 미사용이라.
}

/* 시스템 페이지 크기 (보통 4096 바이트) */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
