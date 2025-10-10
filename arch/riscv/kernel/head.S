/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  
  #bss range:0x50201558~0x50201590
  li t0,0x50201558	#_bss_start
  li t1,0x50201590	#_bss_end

bss_clear:
  bge t0,t1,clear_done
  sd x0,0(t0)		#x0 : 0, 8B
  addi t0,t0,8
  j bss_clear
clear_done:
  /* TODO: [p1-task2] setup C environment */
  li sp,0x50500000	#set stack space
  jal main
loop:
  wfi
  j loop

END(_start)
