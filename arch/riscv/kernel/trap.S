#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0
  /* TODO: [p2-task4] enable interrupts globally */
  li t0,SR_SIE  //全局中断使能
  csrs sstatus, t0
  li t0, SIE_STIE //定时器中断使能
  csrs sie, t0
  ret
ENDPROC(setup_exception)
