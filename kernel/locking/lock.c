#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i=0;i<LOCK_NUM;i++){
        spin_lock_init(&mlocks[i].lock);
        init_list_head(&mlocks[i].block_queue);
        mlocks[i].key = -1;
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
    int expected = UNLOCKED;
    if(atomic_cmpxchg(&lock->status,&expected,LOCKED) == 0){
        return 1;   // acquired
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
            spin_lock_release(&mlocks[i].lock);
            return i;   //return handle
        }
        spin_lock_release(&mlocks[i].lock);
    }
    return 0;   //lock full
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if(mlock_idx < 0 || mlock_idx >= LOCK_NUM){
        return;
    }
    mutex_lock_t *mutex = &mlocks[mlock_idx];

    spin_lock_acquire(&mutex->lock);

    if(!list_empty(&mutex->block_queue)){
        do_block(&current_running->list, &mutex->block_queue);
        spin_lock_release(&mutex->lock);
    }else{
        list_add_tail(&current_running->list, &mutex->block_queue);
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

    list_del(&current_running->list);

    if(!list_empty(&mutex->block_queue)){
        list_node_t *next = mutex->block_queue.next;
        do_unblock(next);

        pcb_t *next_pcb = container_of(next, pcb_t, list);
    }
    spin_lock_release(&mutex->lock);
}
