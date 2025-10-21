#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

#define APP_BASE 0x52000000
#define APP_ADDR_INTERVAL 0x10000

#define TABLE_OFFSET_ADDR 0x502001F8
#define KERNEL_SECTOR_ADDR 0x502001FC
#define TASKNUM_ADDR 0x502001FE

#define MAX_INPUT_LEN 32

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    uint16_t kernel_sectors = *(uint16_t*)KERNEL_SECTOR_ADDR;
    uint16_t tasknum        = *(uint16_t*)TASKNUM_ADDR;
    uint32_t table_offset   = *(uint32_t*)TABLE_OFFSET_ADDR;

    int start_sector = table_offset / SECTOR_SIZE;
    int offset_in_sector = table_offset % SECTOR_SIZE;
    int total_size = sizeof(task_info_t) * tasknum;
    int total_sectors = (offset_in_sector + total_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    
    uint8_t buffer[SECTOR_SIZE * total_sectors];
    bios_sd_read((uintptr_t)buffer, total_sectors, start_sector);
    
    memcpy((void *)tasks, buffer + offset_in_sector, total_size);
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);
    //[p1-task3] load tasks by id
    /*
    int taskid = 0;
    int c;
    while(1){
	    c = bios_getchar();
	    if(c == -1) continue;
	    if(c == '\r' || c == '\n'){
		    bios_putchar('\n');
		    load_task_img(taskid);
		    void (*user_entry)() = (void (*)())((uintptr_t)APP_BASE + taskid * (uintptr_t)APP_ADDR_INTERVAL);
		    user_entry();
    		    bios_putstr("Please input taskid");
		    taskid = 0;
	    }else if(c >= '0' && c<= '9'){
		    bios_putchar(c);
		    taskid = taskid * 10 + (c - '0');
	    }else{
		    bios_putstr("\n[Error] Invalid input.\n");
		    taskid = 0;
	    }
    }
    */

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    
    //[p1-task4] load tasks by name
    char input[MAX_INPUT_LEN];
    int len = 0;

    while (1) {
        int c = bios_getchar();
        if (c == -1) continue;

        //'Enter' for start
        if (c == '\r' || c == '\n') {
            bios_putchar('\n');
            if (len == 0) {
                bios_putstr("No input.\n\r");
                continue;
            }

            input[len] = '\0';  // 字符串结束符

	    uint64_t entry = load_task_img(input);
	    if(entry == 0){
		    bios_putstr("[Error] Failed to load app.\n");
	    }else{
	    	    void (*user_entry)() = (void(*)())entry;
		    user_entry();
		    bios_putstr("Return to kernel\n");
	    }

            //reset buf
            len = 0;
	    int i;
	    for(i = 0;i<MAX_INPUT_LEN;i++){
		    input[i] = 0;
	    }
            bios_putstr("Please input app name to run:\n");
            continue;
        }

        
        if (c >= 32 && c < 127) {
            if (len < MAX_INPUT_LEN - 1) {
                input[len++] = (char)c;
                bios_putchar(c);
            }
        } else {
            bios_putstr("[Error] Invalid input.\n");
            len = 0;
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
