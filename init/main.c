#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define VERSION_BUF 50

#define APP_BASE 0x52000000
#define APP_ADDR_INTERVAL 0x10000

#define TABLE_OFFSET_ADDR 0x502001F8
#define KERNEL_SECTOR_ADDR 0x502001FC
#define TASKNUM_ADDR 0x502001FE

#define MAX_INPUT_LEN 32

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
extern uint16_t kernel_sectors;
extern uint16_t tasknum;
extern uint32_t table_offset;

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first

    int start_sector = table_offset / SECTOR_SIZE;
    int offset_in_sector = table_offset % SECTOR_SIZE;
    int total_size = sizeof(task_info_t) * tasknum;
    int total_sectors = (offset_in_sector + total_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    
    uint8_t buffer[SECTOR_SIZE * total_sectors];
    bios_sd_read((uintptr_t)buffer, total_sectors, start_sector);
    
    memcpy((void *)tasks, buffer + offset_in_sector, total_size);
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    init_list_head(&ready_queue);
    init_list_head(&sleep_queue);
    int i;
    for(i=0;i<tasknum;i++){
        pcb[i].pid = i + 1;
        pcb[i].status = TASK_READY;
        pcb[i].wakeup_time = 0;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;

        pcb[i].kernel_sp = allocKernelPage(1) + PAGE_SIZE;
        pcb[i].user_sp = allocUserPage(1) + PAGE_SIZE;

        list_add_tail(&(pcb[i].list), &ready_queue);
    }

    pid0_pcb.pid = 0;
    pid0_pcb.status = TASK_RUNNING;
    pid0_pcb.wakeup_time = 0;
    pid0_pcb.cursor_x = 0;
    pid0_pcb.cursor_y = 0;
    pid0_pcb.kernel_sp = (reg_t)&pid0_stack + PAGE_SIZE;
    pid0_pcb.user_sp = pid0_pcb.kernel_sp;


    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
    process_id = tasknum + 1;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void)
{
    kernel_sectors = *(uint16_t*)KERNEL_SECTOR_ADDR;
    tasknum        = *(uint16_t*)TASKNUM_ADDR;
    table_offset   = *(uint32_t*)TABLE_OFFSET_ADDR;

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    

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
		    bios_putstr("Return to kernel");
	    }

            //reset buf
            len = 0;
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
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
