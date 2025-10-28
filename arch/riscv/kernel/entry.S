#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
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


  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */


  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */


ENDPROC(exception_handler_entry)
