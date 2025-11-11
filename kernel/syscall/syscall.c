#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    long sysno = regs->regs[17];
    long arg0 = regs->regs[10];
    long arg1 = regs->regs[11];
    long arg2 = regs->regs[12];
    long arg3 = regs->regs[13];
    long arg4 = regs->regs[14];

    long retval = -1;
    if(sysno >= 0 && sysno < NUM_SYSCALLS && syscall[sysno] != NULL){
        retval = syscall[sysno](arg0, arg1, arg2, arg3, arg4);
    }

    regs->regs[10] = retval;

    regs->sepc += 4;    //skip ecall
}
