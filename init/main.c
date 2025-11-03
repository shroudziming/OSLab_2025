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
    jmptab[SYSCALL_YIELD] = (long (*)())do_scheduler;
    jmptab[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[SYSCALL_WRITE] = (long (*)())screen_write;
    jmptab[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
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
    kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    user_stack = allocUserPage(1) + PAGE_SIZE;
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

    for(int i = 0; i < 14; i++){
        pt_switchto->regs[i] = 0;
    }

    

    pt_switchto->regs[0] = (reg_t)entry_point;  //ra
    pt_switchto->regs[1] = (reg_t)kernel_stack;  //sp
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = (reg_t)user_stack;      //pay attention in task3 !!!
    pcb->status = TASK_READY;
    
}
static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    //slot version
    
    init_list_head(&ready_queue);
    init_list_head(&sleep_queue);

    int i;
    for(i=0;i<tasknum;i++){
        pcb[i].pid = -1;
        pcb[i].status = TASK_EXITED;
    }
    
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
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

    bios_putstr("Input task names (one per line), type 'start' to begin scheduling:\n");

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
            if (strcmp(input, "start") == 0) {
                bios_putstr("Starting scheduling...\n\r");
                break;
            }
            uint64_t entry = load_task_img(input);
            if(entry == -1){
                bios_putstr("[Error] Failed to load app.\n");
                
            }else{
                    // void (*user_entry)() = (void(*)())entry;
                    // user_entry();
                    // bios_putstr("Return to kernel");
                    int slot = -1;
                    for(int i = 0; i < tasknum; i++){
                        if(pcb[i].status == TASK_EXITED || pcb[i].pid == -1){
                            slot = i;
                            break;
                        }
                    }
                    if(slot == -1){
                        bios_putstr("[Error] No available slot.\n");
                    } else{
                        pcb_t *p = &pcb[slot];
                        p->pid = process_id++;
                        p->wakeup_time = 0;
                        p->cursor_x = 0;
                        p->cursor_y = 0;

                        ptr_t kernel_sp;
                        ptr_t user_sp;

                        init_pcb_stack(kernel_sp, user_sp, (ptr_t)entry, p);
                        list_add_tail(&p->list, &ready_queue);
                        
                    }
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
        } else if (c == '\b' || c == 127) {  // 退格键处理
                if (len > 0) {
                    len--;
                    bios_putstr("\b \b");  // 回退显示
                }
        } else {
            bios_putstr("[Error] Invalid input.\n");
            len = 0;
        }
    }
    
    init_screen();
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
