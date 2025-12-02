#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>
#include <assert.h>
mutex_lock_t mlocks[LOCK_NUM];
condition_t conditions[CONDITION_NUM];
barrier_t barriers[BARRIER_NUM];
mailbox_t mailboxes[MBOX_NUM];
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
    if(spin_lock_try_acquire(&mlocks[mlock_idx].lock) == 0){
        mlocks[mlock_idx].owner = current_running[cpu_id]->pid;
        mlocks[mlock_idx].locked = 1;
        return;
    }

    do_block(&current_running[cpu_id]->list,&mlocks[mlock_idx].block_queue);
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *mutex = &mlocks[mlock_idx];
    if(!list_empty(&mutex->block_queue)){
        //not empty
        list_node_t *node = mutex->block_queue.next;
        pcb_t *p = get_pcb_by_node(node);
        mutex->owner = p->pid;
        do_unblock(node);
        
    }else {
        mutex->locked = 0;
        mutex->owner = -1;
        spin_lock_release(&mutex->lock);
    }
    
}


//barrier

void init_barriers(void){
    for(int i = 0;i < BARRIER_NUM;i++){
        barriers[i].key = -1;
        barriers[i].used = 0;
        barriers[i].goal = 0;
        barriers[i].count = 0;
        // barriers[i].lock_idx = -1;
        // barriers[i].cond_idx = -1;
        barriers[i].wait_queue.prev = &barriers[i].wait_queue;
        barriers[i].wait_queue.next = &barriers[i].wait_queue;
    }
}

int do_barrier_init(int key, int goal){
    //check if barrier holding key already exists
    for(int i = 0;i < BARRIER_NUM;i++){
        if(barriers[i].key == key && barriers[i].used == 1){
            barriers[i].goal = goal;
            return i;
        }
    }
    //find a free barrier
    for(int i = 0;i <BARRIER_NUM;i++){
        if(barriers[i].used == 0){
            barriers[i].key = key;
            barriers[i].goal = goal;
            return i;
        }
    }
    return -1;  //barrier full
}

void do_barrier_wait(int bar_idx){
    // barrier_t *barrier = &barriers[bar_idx];
    // do_mutex_lock_acquire(barrier->lock_idx);
    // barrier->count++;
    // if(barrier->count < barrier->goal){
    //     do_condition_wait(barrier->cond_idx, barrier->lock_idx);
    // }else{
    //     do_condition_broadcast(barrier->cond_idx);
    //     barrier->count = 0;
    // }

    // do_mutex_lock_release(barrier->lock_idx);
    barrier_t *barrier = &barriers[bar_idx];
    barrier->count++;
    if(barrier->count < barrier->goal){
        do_block(&current_running[cpu_id]->list, &barrier->wait_queue);
    }else{
        free_block_list(&barrier->wait_queue);
        barrier->count = 0;
    }
}

void do_barrier_destroy(int bar_idx){
    // do_condition_destroy(barriers[bar_idx].cond_idx);
    barriers[bar_idx].key = -1;
    barriers[bar_idx].used = 0;
    barriers[bar_idx].goal = 0;
    barriers[bar_idx].count = 0;
    // barriers[bar_idx].lock_idx = -1;
    // barriers[bar_idx].cond_idx = -1;
}

//condition

void init_conditions(void){
    for(int i = 0;i < CONDITION_NUM;i++){
        conditions[i].key = -1;
        conditions[i].used = 0;
        init_list_head(&conditions[i].wait_queue);
    }
}
int do_condition_init(int key){
    for(int i = 0;i < CONDITION_NUM;i++){
        if(conditions[i].key == key && conditions[i].used == 1){
            return i;
        }
    }
    for(int i = 0;i < CONDITION_NUM;i++){
        if(conditions[i].used == 0){
            conditions[i].key = key;
            conditions[i].used = 1;
            return i;
        }
    }
    return -1;   //condition full
}

void do_condition_wait(int cond_idx, int mutex_idx){
    if (cond_idx < 0 || cond_idx >= CONDITION_NUM || 
        mutex_idx < 0 || mutex_idx >= LOCK_NUM) {
        return;
    }
    
    current_running[cpu_id]->status = TASK_BLOCKED;
    list_add_tail(&current_running[cpu_id]->list,&conditions[cond_idx].wait_queue);
    do_mutex_lock_release(mutex_idx);
    do_scheduler();

    // do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx){
    list_node_t *head = &conditions[cond_idx].wait_queue;
    list_node_t *p = head->next;
    if(p != head){
        do_unblock(p);
    }

}

void do_condition_broadcast(int cond_idx){
    condition_t *condition = &conditions[cond_idx];
    free_block_list(&condition->wait_queue);
}

void do_condition_destroy(int cond_idx){
    do_condition_broadcast(cond_idx);
    conditions[cond_idx].key = -1;
    conditions[cond_idx].used = 0;
    init_list_head(&conditions[cond_idx].wait_queue);
}

//mbox

void init_mbox(){
    for(int i = 0;i < MBOX_NUM;i++){
        mailboxes[i].name[0] = '\0';
        mailboxes[i].wp =0;
        mailboxes[i].rp = 0;
        mailboxes[i].count = 0;
        mailboxes[i].wait_empty_queue.prev = &mailboxes[i].wait_empty_queue;
        mailboxes[i].wait_empty_queue.next = &mailboxes[i].wait_empty_queue;
        mailboxes[i].wait_full_queue.prev = &mailboxes[i].wait_full_queue;
        mailboxes[i].wait_full_queue.next = &mailboxes[i].wait_full_queue;
    }
}

int do_mbox_open(char *name){
    for(int i = 0;i < MBOX_NUM;i++){
        if(strcmp(mailboxes[i].name,name) == 0){
            mailboxes[i].count++;
            return i;
        }
    }
    //return a new mailbox
    for(int i = 0;i < MBOX_NUM;i++){
        if(mailboxes[i].name[0] == '\0'){
            strcpy(mailboxes[i].name,name);
            mailboxes[i].count++;
            return i;
        }
    }
    return -1;
}

void do_mbox_close(int mbox_idx){
    mailboxes[mbox_idx].count--;
    if(mailboxes[mbox_idx].count == 0){
        mailboxes[mbox_idx].name[0] = '\0';
        mailboxes[mbox_idx].wp = 0;
        mailboxes[mbox_idx].rp = 0;
    }
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length){
    int temp_w;
    int count = 0;
    //if full
    while((temp_w = mailboxes[mbox_idx].wp + msg_length) > MAX_MBOX_LENGTH + mailboxes[mbox_idx].rp){
        
        do_block(&current_running[cpu_id]->list, &mailboxes[mbox_idx].wait_full_queue);
        count++;
    }
    
    circle_copy(mailboxes[mbox_idx].msg,msg,mailboxes[mbox_idx].wp,msg_length,1);
    mailboxes[mbox_idx].wp = temp_w;
    free_block_list(&mailboxes[mbox_idx].wait_empty_queue); //release processes waiting for msgs

    return count;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    int temp_r;
    int count = 0;
    //if empty
    while((temp_r = mailboxes[mbox_idx].rp + msg_length) > mailboxes[mbox_idx].wp){
        
        do_block(&current_running[cpu_id]->list, &mailboxes[mbox_idx].wait_empty_queue);
        count++;
    }
    circle_copy(msg,mailboxes[mbox_idx].msg,mailboxes[mbox_idx].rp,msg_length,0);
    mailboxes[mbox_idx].rp = temp_r;
    free_block_list(&mailboxes[mbox_idx].wait_full_queue); //release processes waiting for space

    return count;
}

void circle_copy(char *dst, char *src, int dst_idx, int len, int mode){
    //mode = 1 means write
    //mode = 0 means read
    int real_idx;
    if(mode == 1){
        for(int i = 0;i < len;i++){
            real_idx = (dst_idx + i) % MAX_MBOX_LENGTH;
            dst[real_idx] = src[i];     //mailbox <- msg
        }
    }else{
        for(int i = 0;i < len;i++){
            real_idx = (dst_idx + i) % MAX_MBOX_LENGTH;
            dst[i] = src[real_idx];     //msg <- mailbox
        }
    }
}