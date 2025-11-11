#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    uint64_t now = get_timer();
    list_node_t *current, *next;
    current = sleep_queue.next;
    while(current != &sleep_queue){
        next = current->next;

        pcb_t *pcb = container_of(current,pcb_t,list);

        if(now >= pcb->wakeup_time){
            list_del(current);

            pcb->status = TASK_READY;
            pcb->wakeup_time = 0;
            
            list_add(&pcb->list,&ready_queue);
        }

        current = next;
    }
}