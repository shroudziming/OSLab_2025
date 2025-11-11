#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr,type,member) ((type *)((char *)(ptr) - offsetof(type,member)))

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
    pcb_t *next;

    if(prev->pid != 0 && prev->status == TASK_RUNNING){
        prev->status = TASK_READY;
        list_add_tail(&prev->list, &ready_queue);
    }

    if(!list_empty(&ready_queue)){
        list_node_t *next_node = ready_queue.next;
        list_del(next_node);
        next = container_of(next_node, pcb_t, list);
        next->status = TASK_RUNNING;
    } else{
        next = &pid0_pcb;
    }

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, next);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
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
