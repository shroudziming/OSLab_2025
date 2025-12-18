#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/smp.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define USER_STACK_PAGES 1

extern uint16_t kernel_sectors;
extern uint16_t tasknum;
extern uint32_t table_offset;

pcb_t *current_running[NR_CPUS];

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .pgdir = 0xffffffc051000000
};

const ptr_t s_pid0_stack = INIT_KERNEL_STACK + 2 * PAGE_SIZE;
pcb_t s_pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)s_pid0_stack,
    .user_sp = (ptr_t)s_pid0_stack,
    .pgdir = 0xffffffc051000000
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    cpu_id = get_current_cpu_id();
    pcb_t *prev = current_running[cpu_id];

    if(current_running[cpu_id]->pid != -1 && current_running[cpu_id]->pid != 0){
        if(prev->status == TASK_RUNNING){
            prev->status = TASK_READY;
            list_add_tail(&prev->list, &ready_queue);
        }else if(prev->status == TASK_EXITED && prev->pid != -1){
            do_exit();
        }
    }
    
    list_node_t *temp = get_ready_node();
    current_running[cpu_id] = get_pcb_by_node(temp);
    current_running[cpu_id]->status = TASK_RUNNING;
    current_running[cpu_id]->run_cpu_id = cpu_id;
    screen_move_cursor(current_running[cpu_id]->cursor_x, current_running[cpu_id]->cursor_y);

    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid,kva2pa(current_running[cpu_id]->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, current_running[cpu_id]);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.0
    current_running[cpu_id]->wakeup_time = get_timer() + sleep_time;
    
    do_block(&current_running[cpu_id]->list, &sleep_queue);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *pcb = get_pcb_by_node(pcb_node);

    pcb->status = TASK_BLOCKED;

    list_add_tail(pcb_node, queue);
    
    do_scheduler();
    
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    pcb_t *pcb = get_pcb_by_node(pcb_node);

    pcb->status = TASK_READY;

    
    list_add_tail(pcb_node, &ready_queue);
    
}

list_node_t *get_ready_node(){
    list_node_t *temp = ready_queue.next;
    pcb_t *p = get_pcb_by_node(temp);
    if(p->status == TASK_EXITED){
        list_del(temp);
        release_pcb(p);
        return get_ready_node();
    }
    if(temp != &ready_queue){
        list_del(temp);
        return temp;
    }
    return cpu_id ? &s_pid0_pcb.list : &pid0_pcb.list;
}

pid_t do_exec(char *name, int argc, char *argv[]){

    // printk("do_exec: %s\n",name);
    pid_t pid = -1;
    int slot = -1;
    for(int j = 0; j < NUM_MAX_TASK; j++){
        if(pcb[j].status == TASK_EXITED && pcb[j].pid == -1){
            slot = j;
            break;
        }
    }
    if(slot == -1){
        printk("No available slot\n");
        return -1;
    }

    uintptr_t pgdir = pa2kva(allocPage());
    // printk("pgdir at %lx\n",pgdir);
    *(uint64_t *)pgdir = 0;
    // clear page table
    clear_pgdir(pgdir);
    // copy kernel page table
    share_pgtable(pgdir, pa2kva(PGDIR_PA));
    // printk("after share pgdir at %lx\n",pgdir);

    // load & map task (this will alloc pages for text/data/bss as implemented in map_task)
    uint64_t entry_point = map_task(name, pgdir);
    // printk("entry point at %lx\n",entry_point);
    if(entry_point == -1){
        return -1;
    }

    pcb_t *p = &pcb[slot];
    p->pid = process_id;
    p->status = TASK_READY;
    p->cursor_x = 0;
    p->cursor_y = 0;
    p->pgdir = pgdir;
    p->kernel_sp = (reg_t)(pa2kva(allocPage()) + PAGE_SIZE);

    // === setup user stack: pre-allocate USER_STACK_PAGES pages ===
    // allocate pages covering virtual addresses:
    // USER_STACK_ADDR - (USER_STACK_PAGES-1)*PAGE_SIZE  ... USER_STACK_ADDR + PAGE_SIZE
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uintptr_t va = USER_STACK_ADDR - (USER_STACK_PAGES - 1) * PAGE_SIZE + i * PAGE_SIZE;
        alloc_page_helper(va, pgdir); 
    }

    // user virtual top (VA) and working pointer (VA)
    uintptr_t user_top_va = USER_STACK_ADDR + PAGE_SIZE;
    uintptr_t cur_data_va = user_top_va; // will move downward as we push data

    // temporary array to store user virtual addresses of each argv string
    uintptr_t arg_user_va[argc > 0 ? argc : 1];

    // copy argument strings into user stack from high -> low
    for (int i = argc - 1; i >= 0; i--) {
        int len = strlen(argv[i]) + 1;
        cur_data_va -= len; // new start VA for this string
        uintptr_t dst_va = cur_data_va;
        const char *src = argv[i];
        int remaining = len;

        while (remaining > 0) {
            uintptr_t page_va = dst_va & ~(PAGE_SIZE - 1);
            uintptr_t page_kva = alloc_page_helper(page_va, pgdir); // returns KVA for this page
            uintptr_t offset = dst_va & (PAGE_SIZE - 1);
            uintptr_t chunk = PAGE_SIZE - offset;
            if ((uintptr_t)chunk > (uintptr_t)remaining) chunk = remaining;
            memcpy((void *)(page_kva + offset), src, chunk);
            src += chunk;
            dst_va += chunk;
            remaining -= chunk;
        }

        arg_user_va[i] = cur_data_va; // record user VA of this string
    }

    // reserve space for argv pointers (user VA)
    uintptr_t argv_slot_va = cur_data_va - (sizeof(char *) * argc);
    // write argv pointers (user VAs) into argv_slot_va
    for (int i = 0; i < argc; i++) {
        uintptr_t ptr_va = argv_slot_va + i * sizeof(char *);
        uintptr_t page_va = ptr_va & ~(PAGE_SIZE - 1);
        uintptr_t page_kva = alloc_page_helper(page_va, pgdir);
        uintptr_t offset = ptr_va & (PAGE_SIZE - 1);
        uintptr_t val = (uintptr_t)arg_user_va[i];
        memcpy((void *)(page_kva + offset), (const uint8_t *)&val, sizeof(char *));
    }

    // final user stack pointer (align to 128 as original)
    uintptr_t final_user_sp_va = ROUNDDOWN(argv_slot_va, 128);

    // set pcb user_sp (user virtual address)
    p->user_sp = (reg_t)final_user_sp_va;

    // prepare argv pointer (user virtual address) to pass into init_pcb_stack
    char **argv_user_va = (char **)argv_slot_va;

    // list init
    p->list.prev = p->list.next = NULL;

    // call init_pcb_stack with user argv pointer (user VA)
    init_pcb_stack(p->kernel_sp, p->user_sp, entry_point, p, argc, argv_user_va);

    list_add_tail(&p->list, &ready_queue);
    process_id++;
    pid = p->pid;

    return pid;
}

void release_pcb(pcb_t *p){
    if(p == NULL) return;

    pid_t old_pid = p->pid;
    
    if(current_running[0] != p && current_running[1] != p){
        list_del(&p->list);
    }
    
    p->pid = -1;
    release_all_lock(old_pid);

    free_block_list(&p->wait_list);
    //free pages
    if (p->pgdir && p->pgdir != pa2kva(PGDIR_PA)) {
        // printk("free_user_pages for pid=%d pgdir=%lx\n", old_pid, (unsigned long)p->pgdir);
        free_all_pages(p);
        // optionally reset to kernel pgdir as safe default
        p->pgdir = pa2kva(PGDIR_PA);
        // printk("free done\n");
    }
}

void free_block_list(list_head *list){
    list_node_t *p, *next;
    for(p = list->next; p != list; p = next){
        next = p->next;
        do_unblock(p);
    }
}

void release_all_lock(pid_t pid){
    for(int i = 0; i < LOCK_NUM; i++){
        if(mlocks[i].owner == pid && mlocks[i].locked == 1){
            do_mutex_lock_release(i);
        }
    }
}

void do_exit(void){
    current_running[cpu_id]->status = TASK_EXITED;
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    release_pcb(current_running[cpu_id]);
    do_scheduler();
}

int do_kill(pid_t pid){
    pcb_t *p = get_pcb_by_pid(pid);
    if(p == NULL){
        return 0;   //failed
    }
    if(p->status != TASK_EXITED){
        if(p->run_cpu_id != cpu_id){
            // target running on other CPU
            p->status = TASK_EXITED;
        } else {
            // target running on this CPU
            if (p == current_running[cpu_id]) {
                // the process kills itself
                do_exit();
            } else {
                p->status = TASK_EXITED;
            }
        }
    } else {
        // not running: safe to mark and release
        release_pcb(p);
        return 1;
    }

    return 1;   //success
}

int do_waitpid(pid_t pid){
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid){
            if(pcb[i].status != TASK_EXITED){
                do_block(&current_running[cpu_id]->list, &pcb[i].wait_list);
                return pid;
            }
        }
    }
    return 0;
}

void do_process_show(void){
    int i = 0;
    static char *status[] = {"BLOCKED","RUNNING","READY","EXITED"};
    printk("\n[Process Table]\n");
    for(i = 0;i < NUM_MAX_TASK;i++){
        if(pcb[i].status == TASK_EXITED && pcb[i].pid == -1) continue;
        else if(pcb[i].status == TASK_RUNNING){
            printk("[%d] PID: %d STATUS: %s USING: %d\n",i,pcb[i].pid,status[pcb[i].status],pcb[i].run_cpu_id);
        }else{
            printk("[%d] PID: %d STATUS: %s\n",i,pcb[i].pid,status[pcb[i].status]);
        }
    }
}

pid_t do_getpid(){
    return current_running[cpu_id]->pid;
}

pcb_t *get_pcb_by_pid(pid_t pid){
    pcb_t *p;
    for(int i = 0; i < NUM_MAX_TASK; i++){
        p = &pcb[i];
        if(p->pid == pid){
            return p;
        }
    }
    return NULL;
}

pcb_t *get_pcb_by_node(list_node_t *node){
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(&pcb[i].list == node){
            return &pcb[i];
        }
    }
    return cpu_id?&s_pid0_pcb : &pid0_pcb;
}