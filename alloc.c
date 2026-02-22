// alloc.c
#include "alloc.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define HEAP_SIZE   256
#define ALIGNMENT   16
#define CHUNK_SIZE 1<<12
#define MAX(a,b) ((a) > (b) ? (a):(b))

// prototypes
static void mark_free(void* ptr);
static void* coalesce(void* ptr);

// --- 我們的 heap：先用固定陣列，之後再換 mmap ---
static uint8_t heap[HEAP_SIZE];

static uint8_t* heap_start; // heap 起點 (指到 prologue header 的位置)
static uint8_t* heap_end; // 目前 epilogue header 的位址

// 因為要對齊 16 bytes
static const size_t base = 8;

// header/footer 用 8 bytes（uint64_t）存：size + alloc_bit
typedef uint64_t hdr_t;

static size_t align16(size_t x){
    return ((x+(ALIGNMENT-1)) & ~(ALIGNMENT-1));
}

static size_t align16_down(size_t x){
    return (x & ~0xF);
}

static hdr_t pack(size_t size,int alloc) {
    return (hdr_t)(size | (alloc ? 1ULL : 0ULL));
}

static size_t get_size(hdr_t h) {return (size_t)(h & ~0xFULL);}
static int get_alloc(hdr_t h) {return (int)(h & 1ULL);}

// [ Prologue(16,alloc=1) ][ Free block(free_size,alloc=0) ][ Epilogue(0,alloc=1) ]
// block(Prologue and Free block): [header(8bytes) | payload | footer(8bytes)]
// block(Epilogue): [header] 不是一個 block size=0

// Prologue 和 Epilogue 設計不一樣的原因是
// 在 coalesce(bp) 需要讀前一塊的 footer 來知道前一塊大小 
// 如果你正在 free 第一個 real block，那「前一塊」其實不存在
// → 但我們希望程式碼不用特判
// → 所以 Prologue 的 footer 就扮演「前一塊 footer」的角色
// 也就是說：
// Prologue footer 是為了讓 “prev_footer” 永遠存在

// Epilogue 只需要提供「下一塊的 header」
// 同理，free 最後一個 real block 時，你要讀下一塊 header：
// next_alloc = get_alloc(next_header)
// 最後一塊的 next 其實不存在
// → 但我們希望程式碼不用特判
// → 所以 Epilogue 的 header 永遠存在，且 alloc=1
// 也就是： 
// Epilogue header 是為了讓 “next_header” 永遠存在

void* mem_sbrk(intptr_t incr){
    void* allocated_memory = sbrk(incr);

    if(allocated_memory == (void * )-1) {
        perror("sbrk 配置失敗");
        return (void * )-1;
    }

    return allocated_memory;
}

bool  mm_init(void){
    const size_t PROLOGUE_SIZE = 16;    // header+footer
    const size_t EPILOGUE_SIZE = 8;     // header only
    const size_t CHUNK = 4096;          // 初始 chunk 

    uint8_t*  raw = (uint8_t *) mem_sbrk(CHUNK + 16);
    if(raw == (uint8_t*)-1) return false;

    uintptr_t p = (uintptr_t) raw; // 把 raw 這個指標所代表的位址，轉成一個“可以裝得下位址”的無號整數。
    uint8_t* heap_base = (uint8_t *)((p + 15) & ~(uintptr_t)0xF); // 先把 heap base 對齊到 16
    heap_start = heap_base + base; // 讓第一個 header 在 8 mod 16
    // => 第一個 payload = heap_start + 8 會是 16-aligned
    
    // assert(((uintptr_t)(heap_start + 8) & 0xF) == 0);

    // prologue 
    *(hdr_t *) heap_start = pack(PROLOGUE_SIZE,1);
    *(hdr_t *) (heap_start + 8) = pack(PROLOGUE_SIZE,1);

    // Free block
    size_t pad = (uintptr_t)heap_base - (uintptr_t)raw;
    size_t total_avail = (CHUNK + 16) - pad;
    size_t free_size = total_avail - base - PROLOGUE_SIZE - EPILOGUE_SIZE; //( base | Prologue | Epilogue)
    free_size = align16_down(free_size);
    *(hdr_t *) (heap_start + 16) = pack(free_size, 0);
    *(hdr_t *) (heap_start + 16 + free_size - 8) = pack(free_size, 0);

    // Epilogue
    *(hdr_t *) (heap_start + 16 + free_size) = pack(0, 1);

    heap_end = heap_start + 16 + free_size;

    return true;
}


// [ header (8 bytes) | payload (N bytes) | footer (8 bytes) ]
//                    ^
//                    mm_malloc 回傳的 ptr 指到這裡
void  mm_free(void* ptr){
    if(ptr == NULL) return;
    mark_free(ptr);
    ptr = coalesce(ptr);
}

static void mark_free(void* ptr){
    // make header and footer "alloc" equal to false

    // get the block's header(hdr) and footer(ftr)
    hdr_t* hdr = (hdr_t*)((uint8_t*)ptr-8);
    size_t block_size = get_size(*hdr);
    // footer = ptr + block_size - header(8) - footer(8)
    hdr_t* ftr = (hdr_t*)((uint8_t*)ptr+block_size-16);

    // make hdr and ftr "alloc" equal false
    *hdr = pack(block_size, 0);
    *ftr = pack(block_size, 0);
}

static void* coalesce(void* ptr){
    // get the block's header(hdr) and footer(ftr)
    hdr_t* hdr = (hdr_t*)((uint8_t*)ptr-8);
    size_t block_size = get_size(*hdr);
    // footer = ptr + block_size - header(8) - footer(8)
    hdr_t* ftr = (hdr_t*)((uint8_t*)ptr+block_size-16);


    // previous block heaer, footer and size
    hdr_t* prev_ftr = (hdr_t*)((uint8_t*)(hdr)-8);
    size_t prev_size = get_size(*prev_ftr);
    hdr_t* prev_hdr = (hdr_t*)((uint8_t*)(hdr)-prev_size);
    int prev_alloc = get_alloc(*prev_ftr);

    // next block heaer, footer, size  
    hdr_t* next_hdr = (hdr_t*)((uint8_t*)(ftr)+8);
    size_t next_size = get_size(*next_hdr);
    hdr_t* next_ftr = (hdr_t*)((uint8_t*)(next_hdr)+next_size-8);
    int next_alloc = get_alloc(*next_hdr);

    // 4 cases
    
    // prev and next are alloc 
    if(prev_alloc && next_alloc) return ptr;

    // prev not alloc but next alloc 
    // ([h|prev|f] [h|ptr|f]) [[h|next|f]]
    else if(!prev_alloc && next_alloc){
        size_t merged_size = prev_size+block_size;
        
        // change original footer size
        *ftr = pack(merged_size,0);

        // chagne prev header size 
        *prev_hdr = pack(merged_size,0);

        return ((uint8_t*)prev_hdr+8);
    }

    // prev alloc but next not alloc
    // [h|prev|f] ([h|ptr|f] [[h|next|f]])
    else if(prev_alloc && !next_alloc){
        size_t merged_size = block_size+next_size;
        *hdr = pack(merged_size,0);
        *next_ftr = pack(merged_size,0);

        return ptr;
    }

    // prev not alloc and next not alloc
    // ([h|prev|f] [h|ptr|f] [[h|next|f]])
    size_t merged_size =  prev_size+block_size+next_size;
    *prev_hdr = pack(merged_size,0);
    *next_ftr = pack(merged_size,0);

    return ((uint8_t*)prev_hdr+8);
}

// [ 擴展前 ]
// ... | 前一個 Block | 舊 Epilogue (8 bytes) | <-- program break

// [ 擴展後 (sbrk 剛剛執行完) ]
// ... | 前一個 Block | 舊 Epilogue (8 bytes) |--------- extend_size bytes ---------| <-- new break

// [ 重新規劃 (我們真正要做的) ]
// ... | 前一個 Block | 新 Free Header (8)    |... Payload ...| 新 Free Footer (8)  | 新 Epilogue (8) |
//                    ^-------------------- 新的 Free Block (大小剛好是 extend_size)-----------------^
void* extend_heap(size_t require_size){
    size_t extend_size = align16(MAX(require_size, 4096));

    hdr_t* old_Epilogue = (hdr_t *) heap_end;
    
    uint8_t* new_memory = (uint8_t*) mem_sbrk(extend_size);
    if(new_memory == (uint8_t *)-1) return NULL;

    // new free block header (新增加的加上原本 Epilogue 的 8 bytes)
    *old_Epilogue = pack(extend_size, 0);

    // new free block footer
    *(hdr_t *)((uint8_t *)old_Epilogue + extend_size - 8) = pack(extend_size, 0);

    // set new Epilogue and that heap_end point to it
    *(hdr_t *)((uint8_t *)old_Epilogue + extend_size) = pack(0,1);

    heap_end = ((uint8_t *)old_Epilogue + extend_size);

    // coalesce
    return coalesce((uint8_t *)old_Epilogue+8);
}


void* mm_malloc(size_t bytes) {
    // header(8 bytes) + footer(8 bytes)
    const size_t OVERHEAD = 16;
    // the minimun bytes the user require
    const size_t MIN_BLOCK = 32;

    size_t total_size = OVERHEAD + bytes;
    total_size = align16(total_size);
    if(total_size < MIN_BLOCK) total_size = MIN_BLOCK;

    hdr_t* block_header = (hdr_t*)(heap_start + 16);
    while(1){
        size_t block_size = get_size(*block_header);
        int block_alloc = get_alloc(*block_header);
        
        // reach the Epilogue
        if(block_size == 0ULL){  
            hdr_t *new_payload =  extend_heap(total_size);
            if(new_payload == NULL) return NULL;
            block_header = (hdr_t *)((uint8_t *)new_payload - 8); // 重新找一次
            continue;
        }

        if(block_size<total_size || block_alloc==1){
            block_header = (hdr_t*)((uint8_t*)block_header+block_size);
            continue;
        }
        hdr_t* block_footer = (hdr_t*)((uint8_t*)block_header+block_size-8);

        size_t remain_size = block_size-total_size;
        // 需要計算如果分配給 user 的話還剩多少 space ，如果不滿足最小 block size 的話就全部給 user，不然剩下的block(會白白被 header 和 footer 所佔用導致那16bytes永遠不會被用到)
        if(remain_size >= MIN_BLOCK) {
            // block for user 
            hdr_t* user_footer = (hdr_t*)((uint8_t*)block_header+total_size-8);
            *block_header = pack(total_size,1);
            *user_footer = pack(total_size,1);

            // the remain block space
            hdr_t* remain_header = (hdr_t*)((uint8_t*)user_footer+8);
            *remain_header = pack(remain_size,0);
            *block_footer = pack(remain_size,0);

            return (uint8_t*)block_header+8;
        }
        else{
            *block_header = pack(block_size,1); 
            *block_footer = pack(block_size,1); 
            return (uint8_t*)block_header+8;
        }
    }
    return NULL;
}


void  mm_dump(void){
    printf("===== HEAP DUMP =====\n");

    size_t off = 16;
    hdr_t* current = (hdr_t *)(heap_start + 16);

    while(1){
        size_t size = get_size(*current);
        int alloc = get_alloc(*current);

        if(size==0) {
            printf("EPILOGUE at off=%zu (alloc=%d)\n", off, alloc);
            break;
        }

        printf("BLOCK at off=%zu size=%zu alloc=%d\n", off, size, alloc);

        off += size;
        current = (hdr_t *)((uint8_t *)current + size);
    }
    printf("=====================\n");
}