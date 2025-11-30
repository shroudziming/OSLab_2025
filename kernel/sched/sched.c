#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

extern uint16_t kernel_sectors;
extern uint16_t tasknum;
extern uint32_t table_offset;

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
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
    pcb_t *prev = current_running;
    if(current_running->pid != 0 && current_running->pid != -1){
        if(prev->status == TASK_RUNNING){
            prev->status = TASK_READY;
            list_add_tail(&prev->list, &ready_queue);
        }else if(prev->status == TASK_EXITED){
            release_pcb(prev);
        }
    }else{
        prev->status = TASK_READY;
    }
    if(!list_empty(&ready_queue)){
        list_node_t *next_node = ready_queue.next;
        current_running = get_pcb_by_node(next_node);
        list_del(next_node);
    }
    current_running->status = TASK_RUNNING;
    screen_move_cursor(current_running->cursor_x, current_running->cursor_y);

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.0
    current_running->wakeup_time = get_timer() + sleep_time;
    do_block(&current_running->list, &sleep_queue);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *pcb = container_of(pcb_node, pcb_t, list);

    pcb->status = TASK_BLOCKED;
    if(!list_empty(&ready_queue)){
        list_del(pcb_node);
    }

    list_add_tail(pcb_node, queue);
    if(current_running == pcb){
        do_scheduler();
    }
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue

    pcb_t *pcb = container_of(pcb_node, pcb_t, list);

    pcb->status = TASK_READY;

    list_del(pcb_node);
    list_add_tail(pcb_node, &ready_queue);
    
}

pid_t do_exec(char *name, int argc, char *argv[]){
    uint64_t entry = load_task_img(name);
    pid_t pid = -1;
    if(entry == -1){
        printk("Failed to load task %s\n",name);
        return -1;
    }else{
        int slot = -1;
        for(int j = 0; j < NUM_MAX_TASK; j++){
            if(pcb[j].status == TASK_EXITED && pcb[j].pid == -1){
                slot = j;
                break;
            }
        }
        if(slot == -1){
            printk("No available slot\n");
        }else{
            pcb_t *p = &pcb[slot];
            p->pid = process_id;
            p->status = TASK_READY;
            p->cursor_x = 0;
            p->cursor_y = 0;
            p->kernel_sp = (reg_t)(allocKernelPage(1) + PAGE_SIZE);
            p->user_sp = (reg_t)(allocUserPage(1) + PAGE_SIZE);
            uint64_t user_sp = p->user_sp;
            user_sp -= sizeof(char *) * argc;
            char **argv_ptr = (char **)user_sp;
            for(int i = argc - 1;i >= 0;i--){
                int len = strlen(argv[i]) + 1;
                user_sp -= len;
                argv_ptr[i] = (char *)user_sp;
                strcpy((char *)user_sp, argv[i]);
            }
            p->user_sp = (reg_t)ROUNDDOWN(user_sp,128);     // align to 128
            init_pcb_stack(p->kernel_sp, p->user_sp, (ptr_t)entry, p,argc, argv_ptr);
            list_add_tail(&p->list, &ready_queue);
            process_id++;
            pid = p->pid;
        }
    }
    return pid;

}

void release_pcb(pcb_t *p){
    if(p == NULL) return;

    p->status = TASK_EXITED;
    p->pid = -1;
    list_del(&p->list);
    if(!list_empty(&p->wait_list)){
        free_block_list(&p->wait_list);
    }

    release_all_lock(p->pid);

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
    current_running->status = TASK_EXITED;
    do_scheduler();
}

int do_kill(pid_t pid){
    pcb_t *p = get_pcb_by_pid(pid);
    if(p == NULL){
        return 0;   //failed
    }
    if(current_running->pid == pid){
        do_exit();
        return 1;
    }
    p->status = TASK_EXITED;

    release_pcb(p);

    return 1;   //success
}

int do_waitpid(pid_t pid){
    pcb_t *p = get_pcb_by_pid(pid);

    if(p == NULL){
        return 0;   //failed
    }
    if(p->status != TASK_EXITED){
        do_block(&current_running->list, &p->wait_list);
        return p->pid;
    }else{
        return p->pid;
    }
}

void do_process_show(void){
    int i = 0;
    static char *status[] = {"BLOCKED","RUNNING","READY","EXITED"};
    printk("\n[Process Table]\n");
    for(i = 0;i < NUM_MAX_TASK;i++){
        if(pcb[i].status == TASK_EXITED && pcb[i].pid == -1) continue;
        else{
            printk("[%d] PID: %d STATUS: %s\n",i,pcb[i].pid,status[pcb[i].status]);
        }
    }
}

pid_t do_getpid(){
    return current_running->pid;
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
    return &pid0_pcb;
}