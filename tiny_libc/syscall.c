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

    return 0;
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

void sys_write(char *buff)
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
    return invoke_syscall(MUTEX_INIT, (long)key, IGNORE, IGNORE, IGNORE, IGNORE);
    // return 0;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    // call_jmptab(MUTEX_ACQ,(long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(MUTEX_ACQ, (long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    // call_jmptab(MUTEX_RELEASE,(long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(MUTEX_RELEASE, (long)mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
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
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/