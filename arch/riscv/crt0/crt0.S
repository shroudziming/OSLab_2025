#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
	
	la sp,_start
	li t0,0x8000
	add sp,sp,t0
	
	addi sp,sp,-8
	sd ra,0(sp)

	la t1,__bss_start
	la t2,__BSS_END__
bss_clear:
	bge t1,t2,bss_done
	sd zero,0(t1)
	addi t1,t1,8
	j bss_clear
bss_done:

    /* TODO: [p1-task3] enter main function */
	call main

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
	ld ra,0(sp)
	addi sp,sp,8
	ret
    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
