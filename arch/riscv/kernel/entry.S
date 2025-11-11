#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  addi sp, sp, -(OFFSET_SIZE)

  sd x0, OFFSET_REG_ZERO(sp)
  sd x1, OFFSET_REG_RA(sp)
  sd x2, OFFSET_REG_SP(sp)
  sd x3, OFFSET_REG_GP(sp)
  sd x4, OFFSET_REG_TP(sp)
  sd x5, OFFSET_REG_T0(sp)
  sd x6, OFFSET_REG_T1(sp)
  sd x7, OFFSET_REG_T2(sp)
  sd x8, OFFSET_REG_S0(sp)
  sd x9, OFFSET_REG_S1(sp)
  sd x10, OFFSET_REG_A0(sp)
  sd x11, OFFSET_REG_A1(sp)
  sd x12, OFFSET_REG_A2(sp)
  sd x13, OFFSET_REG_A3(sp)
  sd x14, OFFSET_REG_A4(sp)
  sd x15, OFFSET_REG_A5(sp)
  sd x16, OFFSET_REG_A6(sp)
  sd x17, OFFSET_REG_A7(sp)
  sd x18, OFFSET_REG_S2(sp)
  sd x19, OFFSET_REG_S3(sp)
  sd x20, OFFSET_REG_S4(sp)
  sd x21, OFFSET_REG_S5(sp)
  sd x22, OFFSET_REG_S6(sp)
  sd x23, OFFSET_REG_S7(sp)
  sd x24, OFFSET_REG_S8(sp)
  sd x25, OFFSET_REG_S9(sp)
  sd x26, OFFSET_REG_S10(sp)
  sd x27, OFFSET_REG_S11(sp)
  sd x28, OFFSET_REG_T3(sp)
  sd x29, OFFSET_REG_T4(sp)
  sd x30, OFFSET_REG_T5(sp)
  sd x31, OFFSET_REG_T6(sp)
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  csrc sstatus, t0

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  csrr t0, sstatus
  sd t0, OFFSET_REG_SSTATUS(sp)
  cssr t0, sepc
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, stval
  sd t0, OFFSET_REG_SBADADDR(sp)
  csrr t0, scause
  sd t0, OFFSET_REG_SCASE(sp)
.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw sstatus, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw sepc, t0

  //restore general purpose registers(sp last)
  ld x0, OFFSET_REG_ZERO(sp)
  ld x1, OFFSET_REG_RA(sp)
  ld x3, OFFSET_REG_GP(sp)
  ld x4, OFFSET_REG_TP(sp)
  ld x5, OFFSET_REG_T0(sp)
  ld x6, OFFSET_REG_T1(sp)
  ld x7, OFFSET_REG_T2(sp)
  ld x8, OFFSET_REG_S0(sp)
  ld x9, OFFSET_REG_S1(sp)
  ld x10, OFFSET_REG_A0(sp)
  ld x11, OFFSET_REG_A1(sp)
  ld x12, OFFSET_REG_A2(sp)
  ld x13, OFFSET_REG_A3(sp)
  ld x14, OFFSET_REG_A4(sp)
  ld x15, OFFSET_REG_A5(sp)
  ld x16, OFFSET_REG_A6(sp)
  ld x17, OFFSET_REG_A7(sp)
  ld x18, OFFSET_REG_S2(sp)
  ld x19, OFFSET_REG_S3(sp)
  ld x20, OFFSET_REG_S4(sp)
  ld x21, OFFSET_REG_S5(sp)
  ld x22, OFFSET_REG_S6(sp)
  ld x23, OFFSET_REG_S7(sp)
  ld x24, OFFSET_REG_S8(sp)
  ld x25, OFFSET_REG_S9(sp)
  ld x26, OFFSET_REG_S10(sp)
  ld x27, OFFSET_REG_S11(sp)
  ld x28, OFFSET_REG_T3(sp)
  ld x29, OFFSET_REG_T4(sp)
  ld x30, OFFSET_REG_T5(sp)
  ld x31, OFFSET_REG_T6(sp)

  ld x2, OFFSET_REG_SP(sp)
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  sd ra, 0(sp)
  sd sp, 8(sp)
  sd s0, 16(sp)
  sd s1, 24(sp)
  sd s2, 32(sp)
  sd s3, 40(sp)
  sd s4, 48(sp)
  sd s5, 56(sp)
  sd s6, 64(sp)
  sd s7, 72(sp)
  sd s8, 80(sp)
  sd s9, 88(sp)
  sd s10, 96(sp)
  sd s11, 104(sp)

  sd sp,0(a0) /* save current sp */

  ld sp,0(a1) /* load next sp */

  mv tp,a1 /* current_running */
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  ld ra, 0(sp)
  ld sp, 8(sp)
  ld s0, 16(sp)
  ld s1, 24(sp)
  ld s2, 32(sp)
  ld s3, 40(sp)
  ld s4, 48(sp)
  ld s5, 56(sp)
  ld s6, 64(sp)
  ld s7, 72(sp)
  ld s8, 80(sp)
  ld s9, 88(sp)
  ld s10, 96(sp)
  ld s11, 104(sp)

  addi sp, sp, SWITCH_TO_SIZE
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  la ra, ret_from_exception

  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don not forget to pass parameters for it.
   */
  csrr a0,sp      // regs_context_t
  csrr a1,stval   // sbadaddr
  csrr a2,scause  // scause


ENDPROC(exception_handler_entry)
