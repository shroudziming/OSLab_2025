#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr,type,member) ((type *)((char *)(ptr) - offsetof(type,member)))

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i=0;i<LOCK_NUM;i++){
        spin_lock_init(&mlocks[i].lock);
        init_list_head(&mlocks[i].block_queue);
        mlocks[i].key = -1;
        mlocks[i].owner = -1;
        mlocks[i].locked = 0;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    uint32_t expected = LOCKED;

    uint32_t ret = atomic_swap(expected,(ptr_t)&(lock->status));
    if(ret == UNLOCKED){
        return 1;   //success
    }
    return 0;   //failed
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while(!spin_lock_try_acquire(lock)){
        ;
    }    
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    //check if the key exists
    for(int i = 0;i<LOCK_NUM;i++){
        spin_lock_acquire(&mlocks[i].lock);
        if(mlocks[i].key == key){
            spin_lock_release(&mlocks[i].lock);
            return i;   //return handle
        }
        spin_lock_release(&mlocks[i].lock);
    }

    for(int i = 0;i <LOCK_NUM;i++){
        spin_lock_acquire(&mlocks[i].lock);
        if(mlocks[i].key == -1){
            mlocks[i].key = key;
            init_list_head(&mlocks[i].block_queue);
            mlocks[i].owner = -1;
            mlocks[i].locked = 0;
            spin_lock_release(&mlocks[i].lock);
            return i;   //return handle
        }
        spin_lock_release(&mlocks[i].lock);
    }
    return -1;   //lock full
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if(mlock_idx < 0 || mlock_idx >= LOCK_NUM){
        return;
    }
    mutex_lock_t *mutex = &mlocks[mlock_idx];

    spin_lock_acquire(&mutex->lock);

    // if(!list_empty(&mutex->block_queue)){
    //     do_block(&current_running->list, &mutex->block_queue);
    //     spin_lock_release(&mutex->lock);
    // }else{
    //     list_add_tail(&current_running->list, &mutex->block_queue);
    //     spin_lock_release(&mutex->lock);
    // }

    if (mutex->locked) {
        // 锁被占用，加入等待队列并阻塞
        list_add_tail(&current_running->list, &mutex->block_queue);
        
        spin_lock_release(&mutex->lock);

        do_block(&current_running->list, &mutex->block_queue);
    } else {
        // 锁空闲，获得锁
        mutex->locked = 1;
        mutex->owner = current_running->pid;
        spin_lock_release(&mutex->lock);
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    if(mlock_idx < 0 || mlock_idx >= LOCK_NUM){
        return;
    }
    mutex_lock_t *mutex = &mlocks[mlock_idx];

    spin_lock_acquire(&mutex->lock);

    if(!list_empty(&mutex->block_queue)){
        //取出第一个等待者
        list_node_t *next_wait = mutex->block_queue.next;
        pcb_t *next_pcb = container_of(next_wait, pcb_t, list);

        if(next_pcb != NULL && next_pcb->status == TASK_BLOCKED){
            mutex->owner = next_pcb->pid;
            mutex->locked = 1;
            do_unblock(next_wait);
        } else {
            mutex->locked = 0;
            mutex->owner = -1;
        }
    }else {
        mutex->locked = 0;
        mutex->owner = -1;
    }
    spin_lock_release(&mutex->lock);
}


//barrier

void init_barriers(void){

}

int do_barrier_init(int key, int goal){

}

void do_barrier_wait(int bar_idx){

}

void do_barrier_destroy(int bar_idx){

}

//condition

void init_conditions(void){
    
}
int do_condition_init(int key){

}

void do_condition_wait(int cond_idx, int mutex_idx){

}

void do_condition_signal(int cond_idx){

}

void do_condition_broadcast(int cond_idx){

}

void do_condition_destroy(int cond_idx){

}

//mbox

void init_mbox(){

}

int do_mbox_open(char *name){

}

void do_mbox_close(int mbox_idx){

}

int do_mbox_send(int mbox_idx, void * msg, int msg_length){

}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    
}