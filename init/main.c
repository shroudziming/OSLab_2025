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
#include <os/smp.h>
#include <os/ioremap.h>
#include <os/net.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <plic.h>


#define VERSION_BUF 50

#define APP_BASE 0xffffffc052000000
#define APP_ADDR_INTERVAL 0x10000

#define TABLE_OFFSET_ADDR 0xffffffc0502001F8
#define KERNEL_SECTOR_ADDR 0xffffffc0502001FC
#define TASKNUM_ADDR 0xffffffc0502001FE
#define SWAP_START 0xffffffc0502001f0
#define MAX_INPUT_LEN 32


int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
extern uint16_t kernel_sectors;
extern uint16_t tasknum;
extern uint32_t table_offset;
extern uint32_t swap_start_sector;

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
    // jmptab[SYSCALL_YIELD] = (long (*)())do_scheduler;
    // jmptab[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    // jmptab[SYSCALL_WRITE] = (long (*)())screen_write;
    // jmptab[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
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
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb,int argc, char *argv[])
{   
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_regs->regs[1] = (reg_t)entry_point;   //ra
    pt_regs->regs[2] = (reg_t)user_stack;   //sp
    pt_regs->regs[4] = (uint64_t)pcb;     //tp
    pt_regs->sepc = (uint64_t)entry_point;

    //a0:argc,a1:argv
    pt_regs->regs[10] = (reg_t)argc;    //a0
    pt_regs->regs[11] = (reg_t)argv;    //a1

    //set sstatus
    pt_regs->sstatus = SR_SPIE | SR_SUM; //enable user mode interrupt & allow user to access sup page

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    for(int i = 0; i < 14; i++){
        pt_switchto->regs[i] = 0;
    }
    pcb->kernel_sp = kernel_stack - sizeof(switchto_context_t) - sizeof(regs_context_t);
    pt_switchto->regs[0] = (reg_t)ret_from_exception;  //ra
    pt_switchto->regs[1] = pcb->kernel_sp;  //sp
    
}
static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    //slot version
    
    init_list_head(&ready_queue);
    init_list_head(&sleep_queue);

    int i;
    for(i=0;i<NUM_MAX_TASK;i++){
        pcb[i].pid = -1;
        pcb[i].status = TASK_EXITED;
        pcb[i].run_cpu_id = 0;
        init_list_head(&pcb[i].wait_list);
        pcb[i].list.prev = pcb[i].list.next = NULL;
    }
    
    /* TODO: [p2-task1] remember to initialize 'current_running[cpu_id]' */
    pid0_pcb.status = TASK_RUNNING;
    init_list_head(&pid0_pcb.list);
    s_pid0_pcb.status = TASK_READY;
    init_list_head(&s_pid0_pcb.list);
    init_pcb_stack(pid0_pcb.kernel_sp, pid0_pcb.user_sp, (uint64_t)ret_from_exception, &pid0_pcb,0,NULL);
    init_pcb_stack(s_pid0_pcb.kernel_sp, s_pid0_pcb.user_sp, (uint64_t)ret_from_exception, &s_pid0_pcb,0,NULL);
    current_running[0] = &pid0_pcb;
    current_running[1] = &s_pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]          = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]          = (long (*)())do_scheduler;
    syscall[SYSCALL_CURSOR]         = (long (*)())screen_move_cursor;
    syscall[SYSCALL_CLEAR]          = (long (*)())screen_clear;
    syscall[SYSCALL_WRITE]          = (long (*)())screen_write;
    syscall[SYSCALL_REFLUSH]        = (long (*)())screen_reflush;
    syscall[SYSCALL_LOCK_INIT]      = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]       = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]   = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_GET_TICK]       = (long (*)())get_ticks;
    syscall[SYSCALL_GET_TIMEBASE]   = (long (*)())get_time_base;
    syscall[SYSCALL_EXEC]           = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]           = (long (*)())do_exit;
    syscall[SYSCALL_WAITPID]        = (long (*)())do_waitpid;
    syscall[SYSCALL_KILL]           = (long (*)())do_kill;
    syscall[SYSCALL_PS]             = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]         = (long (*)())do_getpid;
    syscall[SYSCALL_READCH]         = (long (*)())bios_getchar;
    syscall[SYSCALL_BARR_INIT]      = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]      = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT]      = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]    = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]   = (long (*)())do_condition_destroy;
    // syscall[SYSCALL_SEMA_INIT]   = (long (*)())do_semaphore_init;
    // syscall[SYSCALL_SEMA_UP]     = (long (*)())do_semaphore_up;
    // syscall[SYSCALL_SEMA_DOWN]   = (long (*)())do_semaphore_down;
    // syscall[SYSCALL_SEMA_DESTROY]= (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_MBOX_OPEN]      = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long (*)())do_mbox_recv;

    syscall[SYSCALL_PUTCHAR]        = (long (*)())screen_putchar;
}
/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

void disable_temp_map(){
    PTE *pgdir = (PTE *)pa2kva(PGDIR_PA);
    for(uint64_t va = 0x50000000lu;va < 0x51000000lu;va += 0x200000lu){
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
        pmd[vpn1] = 0;  //先清除2MB页表项
    }
    pgdir[1] = 0;   //clear temporary mapping in 0x50000000 ~ 0x51000000
}


int main(void)
{
    kernel_sectors = *(uint16_t*)KERNEL_SECTOR_ADDR;
    tasknum        = *(uint16_t*)TASKNUM_ADDR;
    table_offset   = *(uint32_t*)TABLE_OFFSET_ADDR;
    swap_start_sector = *(uint32_t*)SWAP_START;
    int cid = get_current_cpu_id();

    if(cid == 0){
        smp_init();

        lock_kernel();

        init_memory_manager();

        init_alloc_info();

        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();
        
        // TODO: [p5-task4] Init plic
        // plic_init(plic_addr, nr_irqs);
        // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");


        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        init_conditions();

        init_barriers();

        init_mbox();
        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");
        
        
        printk("> [INIT] shell initialization succeeded.\n");
        unlock_kernel();
        wakeup_other_hart();

        lock_kernel();  //re grab the lock

        printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        (unsigned int)get_current_cpu_id());

        disable_temp_map();
        cpu_id = 0;

        pid_t shell_pid = do_exec("shell",0,NULL);
    }else{
        lock_kernel();
        cpu_id = 1;
        current_running[cpu_id]->status = TASK_RUNNING;

        printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        (unsigned int)get_current_cpu_id());

        unlock_kernel();
    }
    
    setup_exception();

    /*
     * Just start kernel with VM and print this string
     * in the first part of task 1 of project 4.
     * NOTE: if you use SMP, then every CPU core should call
     *  `kernel_brake()` to stop executing!
     */
    // printk("> [INIT] CPU #%u has entered kernel with VM!\n",
    //     (unsigned int)get_current_cpu_id());
    // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.
    // kernel_brake();

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer(get_ticks() + TIMER_INTERVAL);

    printk("> [INIT] CPU %d initialization succeeded.\n", cpu_id);
    // printk("swap starts from sector %d\n", swap_start_sector);

    unlock_kernel();
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
