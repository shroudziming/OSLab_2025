#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long ret;
    asm volatile("mv a7, %[sysno]\n"
                 "mv a0, %[arg0]\n"
                 "mv a1, %[arg1]\n"
                 "mv a2, %[arg2]\n"
                 "mv a3, %[arg3]\n"
                 "mv a4, %[arg4]\n"
                 "ecall\n"
                 "mv %[ret], a0\n"
                 :[ret] "=r"(ret)
                 :[sysno] "r"(sysno), [arg0] "r"(arg0), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3), [arg4] "r"(arg4)
                 : "a0","a1","a2","a3","a4","a7","memory"
    );

    return ret;
}

void sys_yield(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    // call_jmptab(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    // call_jmptab(SYSCALL_CURSOR,(long)x,(long)y, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, (long)x, (long)y, IGNORE, IGNORE, IGNORE);
}

void sys_clear(void)
{
    invoke_syscall(SYSCALL_CLEAR,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}
void sys_screen_write(char *buff)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    // call_jmptab(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    // call_jmptab(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    // return call_jmptab(MUTEX_INIT,(long)key, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    return invoke_syscall(SYSCALL_LOCK_INIT, (long)key, IGNORE, IGNORE, IGNORE, IGNORE);
    // return 0;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    // call_jmptab(MUTEX_ACQ,(long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, (long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    // call_jmptab(MUTEX_RELEASE,(long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, (long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, (long)time, IGNORE, IGNORE, IGNORE, IGNORE);
}

/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    return (pid_t)invoke_syscall(SYSCALL_EXEC,(long)name,(long)argc,(long)argv,IGNORE,IGNORE);
}
#endif

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    return (int)invoke_syscall(SYSCALL_KILL,(long)pid,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    return (int)invoke_syscall(SYSCALL_WAITPID,(long)pid,IGNORE,IGNORE,IGNORE,IGNORE);
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implemGent sys_ps */
    invoke_syscall(SYSCALL_PS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    return (pid_t)invoke_syscall(SYSCALL_GETPID,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    return (int)invoke_syscall(SYSCALL_READCH,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_putchar(char ch)
{
    //added by myself
    invoke_syscall(SYSCALL_PUTCHAR,(long)ch,IGNORE,IGNORE,IGNORE,IGNORE);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    return (int)invoke_syscall(SYSCALL_BARR_INIT,(long)key,(long)goal,IGNORE,IGNORE,IGNORE);
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT,(long)bar_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY,(long)bar_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    return (int)invoke_syscall(SYSCALL_COND_INIT,(long)key,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT,(long)cond_idx,(long)mutex_idx,IGNORE,IGNORE,IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL,(long)cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST,(long)cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY,(long)cond_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
    return (int)invoke_syscall(SYSCALL_SEMA_INIT,(long)key,(long)init,IGNORE,IGNORE,IGNORE);
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
    invoke_syscall(SYSCALL_SEMA_UP,(long)sema_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
    invoke_syscall(SYSCALL_SEMA_DOWN,(long)sema_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
    invoke_syscall(SYSCALL_SEMA_DESTROY,(long)sema_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    return (int)invoke_syscall(SYSCALL_MBOX_OPEN,(long)name,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE,(long)mbox_id,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    return (int)invoke_syscall(SYSCALL_MBOX_SEND,(long)mbox_idx,(long)msg,(long)msg_length,IGNORE,IGNORE);

}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    return (int)invoke_syscall(SYSCALL_MBOX_RECV,(long)mbox_idx,(long)msg,(long)msg_length,IGNORE,IGNORE);
}

int sys_pipe_open(const char *name)
{
    return (int)invoke_syscall(SYSCALL_PIPE_OPEN,(long)name,IGNORE,IGNORE,IGNORE,IGNORE);
}

long sys_pipe_give_pages(int pipe_idx , void *src, size_t length)
{
    return invoke_syscall(SYSCALL_PIPE_GIVE,(long)pipe_idx,(long)src,(long)length,IGNORE,IGNORE);
}

long sys_pipe_take_pages(int pipe_idx , void *dst, size_t length)
{
    return invoke_syscall(SYSCALL_PIPE_TAKE,(long)pipe_idx,(long)dst,(long)length,IGNORE,IGNORE);
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    return invoke_syscall(SYSCALL_NET_SEND,(long)txpacket,(long)length,IGNORE,IGNORE,IGNORE);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    return invoke_syscall(SYSCALL_NET_RECV,(long)rxbuffer,(long)pkt_num,(long)pkt_lens,IGNORE,IGNORE);
}

int sys_mkfs(int force)
{
    // TODO [P6-task1]: Implement sys_mkfs
    return invoke_syscall(SYSCALL_FS_MKFS,(long)force,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_mkfs succeeds
}

int sys_statfs(void)
{
    // TODO [P6-task1]: Implement sys_statfs
    return invoke_syscall(SYSCALL_FS_STATFS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_statfs succeeds
}

int sys_cd(char *path)
{
    // TODO [P6-task1]: Implement sys_cd
    return invoke_syscall(SYSCALL_FS_CD,(long)path,IGNORE,IGNORE,IGNORE,IGNORE);  // sys_cd succeeds
}

int sys_mkdir(char *path)
{
    // TODO [P6-task1]: Implement sys_mkdir
    return invoke_syscall(SYSCALL_FS_MKDIR, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_mkdir succeeds
}

int sys_rmdir(char *path)
{
    // TODO [P6-task1]: Implement sys_rmdir
    return invoke_syscall(SYSCALL_FS_RMDIR, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_rmdir succeeds
}

int sys_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement sys_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    return invoke_syscall(SYSCALL_FS_LS, (long)path, (long)option, IGNORE, IGNORE, IGNORE);  // sys_ls succeeds
}

int sys_touch(char *path)
{
    // TODO [P6-task2]: Implement sys_touch
    return invoke_syscall(SYSCALL_FS_TOUCH, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_touch succeeds
}

int sys_cat(char *path)
{
    // TODO [P6-task2]: Implement sys_cat
    return invoke_syscall(SYSCALL_FS_CAT, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_cat succeeds
}

int sys_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement sys_open
    return invoke_syscall(SYSCALL_FS_OPEN, (long)path, (long)mode, IGNORE, IGNORE, IGNORE);  // return the id of file descriptor
}

int sys_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_read
    return invoke_syscall(SYSCALL_FS_READ, (long)fd, (long)buff, (long)length, IGNORE, IGNORE);  // return the length of trully read data
}

int sys_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_write
    return invoke_syscall(SYSCALL_FS_WRITE, (long)fd, (long)buff, (long)length, IGNORE, IGNORE);  // return the length of trully written data
}

int sys_close(int fd)
{
    // TODO [P6-task2]: Implement sys_close
    return invoke_syscall(SYSCALL_FS_CLOSE, (long)fd, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_close succeeds
}

int sys_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement sys_ln
    return invoke_syscall(SYSCALL_FS_LN, (long)src_path, (long)dst_path, IGNORE, IGNORE, IGNORE);  // sys_ln succeeds 
}

int sys_rm(char *path)
{
    // TODO [P6-task2]: Implement sys_rm
    return invoke_syscall(SYSCALL_FS_RM, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);  // sys_rm succeeds 
}

int sys_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement sys_lseek
    return invoke_syscall(SYSCALL_FS_LSEEK, (long)fd, (long)offset, (long)whence, IGNORE, IGNORE);  // the resulting offset location from the beginning of the file
}
/************************************************************/
