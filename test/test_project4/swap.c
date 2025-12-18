#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#define WORKING_SET_PAGES 270

static uint8_t buffer[WORKING_SET_PAGES * PAGE_SIZE];

int main()
{
    printf("FIFO page fault demo start\n");

    for (int i = 0; i < WORKING_SET_PAGES; i++) {
        buffer[i * PAGE_SIZE] = i;
    }

    printf("Initial page faults done\n");

    for (int round = 0; round < 1000; round++) {
        for (int i = 0; i < WORKING_SET_PAGES; i++) {
            buffer[i * PAGE_SIZE]++;
        }
        sys_sleep(10);
    }

    printf("Finished normally\n");
    return 0;
}
