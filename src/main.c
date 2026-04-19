#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define NUM_PTRS 500       // 同時最多持有 500 個指標
#define ITERATIONS 100000  // 進行 10 萬次隨機操作
#define MAX_ALLOC_SIZE 4096 // 最大隨機分配 4KB

void run_stress_test() {
    printf("===== 🚀 啟動壓力測試 =====\n");
    
    // 用來存放指標與對應的大小
    void* ptrs[NUM_PTRS] = {NULL};
    size_t sizes[NUM_PTRS] = {0};

    srand(time(NULL)); // 初始化亂數種子

    for (int i = 0; i < ITERATIONS; i++) {
        // 隨機挑選一個操作欄位
        int idx = rand() % NUM_PTRS;

        if (ptrs[idx] == NULL) {
            // --- 動作：Malloc ---
            // 隨機決定要分配的大小 (1 ~ MAX_ALLOC_SIZE bytes)
            size_t sz = (rand() % MAX_ALLOC_SIZE) + 1;
            
            ptrs[idx] = mm_malloc(sz);
            sizes[idx] = sz;

            if (ptrs[idx] != NULL) {
                // 為了測試資料完整性，我們把這塊記憶體填滿特定的 pattern (用 idx 當作標記)
                memset(ptrs[idx], (uint8_t)(idx & 0xFF), sz);
            } else {
                // 如果回傳 NULL，可能是 sbrk 失敗或系統沒記憶體了
                printf("Warning: mm_malloc returned NULL for size %zu\n", sz);
            }
        } else {
            // --- 動作：Free ---
            // 1. 在 free 之前，先檢查資料有沒有被其他惡意的 block 踩踏！
            uint8_t* payload = (uint8_t*)ptrs[idx];
            uint8_t expected_pattern = (uint8_t)(idx & 0xFF);
            
            for (size_t j = 0; j < sizes[idx]; j++) {
                if (payload[j] != expected_pattern) {
                    printf("💥 災難發生！資料遭到破壞！在 idx %d, offset %zu\n", idx, j);
                    exit(1);
                }
            }

            // 2. 資料沒問題，放心釋放
            mm_free(ptrs[idx]);
            ptrs[idx] = NULL;
            sizes[idx] = 0;
        }

        // 定期印出進度，讓你確認程式沒有卡死
        if (i > 0 && i % 20000 == 0) {
            printf("  已完成 %d 次隨機操作...\n", i);
        }
    }

    // 測試結束，把剩下的指標全部 free 掉
    printf("正在清理殘留的記憶體...\n");
    for (int i = 0; i < NUM_PTRS; i++) {
        if (ptrs[i] != NULL) {
            mm_free(ptrs[i]);
        }
    }

    printf("===== 🎉 壓力測試完美通過！ =====\n");
}

int main(void) {
    if (!mm_init()) {
        printf("mm_init failed\n");
        return 1;
    }

    // 跑完你原本的基礎測試後，呼叫壓力測試
    run_stress_test();

    return 0;
}