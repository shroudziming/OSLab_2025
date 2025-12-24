#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#define PAGE_SIZE 4096
#define TEST_PAGES 6   //大于等于4，以触发换出
#define BASE_VA    0x40000000

int main() {
    printf("FIFO swap test begin...\n");
    printf("We will test %d pages, which is more than KERN_PAGE_MAX_NUM (%d).\n", TEST_PAGES, 4);
    volatile uint8_t *p;

    // Round 1: 写入数据，填满物理页并触发换出
    printf("\n[Round 1] Writing initial values to %d pages...\n", TEST_PAGES);
    for (int i = 0; i < TEST_PAGES; i++) {
        p = (uint8_t *)(BASE_VA + i * PAGE_SIZE);
        *p = (uint8_t)i; // 写入一个可验证的唯一值
        printf("  > Wrote value %d to address 0x%lx\n", *p, (uintptr_t)p);
    }
    printf("[Round 1] Done.\n");


    // Round 2: 校验数据，触发页面换入
    printf("\n[Round 2] Verifying values of all %d pages...\n", TEST_PAGES);
    int errors = 0;
    for (int i = 0; i < TEST_PAGES; i++) {
        p = (uint8_t *)(BASE_VA + i * PAGE_SIZE);
        uint8_t expected_value = (uint8_t)i;
        uint8_t actual_value = *p;

        printf("  > Verifying address 0x%lx... ", (uintptr_t)p);
        if (actual_value == expected_value) {
            printf("OK (value is %d)\n", actual_value);
        } else {
            printf("ERROR! Expected %d, but got %d\n", expected_value, actual_value);
            errors++;
        }
    }

    //Round 3:再次读最后几个页面，不触发换页
    printf("\n[Round 3] Re-verifying last 3 pages to avoid swap...\n");
    for(int i = TEST_PAGES - 2;i < TEST_PAGES;i++){
        p = (uint8_t *)(BASE_VA + i * PAGE_SIZE);
        uint8_t expected_value = (uint8_t)i;
        uint8_t actual_value = *p;
        printf("  > Verifying address 0x%lx... ", (uintptr_t)p);
        if (actual_value == expected_value) {
            printf("OK (value is %d)\n", actual_value);
        } else {
            printf("ERROR! Expected %d, but got %d\n", expected_value, actual_value);
            errors++;
        }
    }

    if (errors == 0) {
        printf("\nFIFO swap test passed!\n");
    } else {
        printf("\nFIFO swap test failed with %d errors.\n", errors);
    }

    return 0;
}