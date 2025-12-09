/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0xffffffc050501000
#define S_KERNEL_STACK	0xffffffc050502000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bnez t0, s_cpu_start

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  
  #bss range:0x50201558~0x50201590
  la t0,__bss_start	#_bss_start
  la t1,__BSS_END__	#_bss_end

bss_clear:
  bge t0,t1,clear_done
  sd x0,0(t0)		#x0 : 0, 8B
  addi t0,t0,8
  j bss_clear
clear_done:
  /* TODO: [p1-task2] setup C environment */

  la tp,pid0_pcb
  li sp,KERNEL_STACK	#set stack space
  call main

s_cpu_start:
  la tp,s_pid0_pcb
  li sp,S_KERNEL_STACK
  call main

loop:
  wfi
  j loop

END(_start)
