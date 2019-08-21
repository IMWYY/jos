/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(t_divide, T_DIVIDE)   	// 0
TRAPHANDLER_NOEC(t_debug, T_DEBUG)        // 1
TRAPHANDLER_NOEC(t_nmi, T_NMI)            // 2
TRAPHANDLER_NOEC(t_brkpt, T_BRKPT)        // 3
TRAPHANDLER_NOEC(t_oflow, T_OFLOW)        // 4
TRAPHANDLER_NOEC(t_bound, T_BOUND)        // 5
TRAPHANDLER_NOEC(t_illop, T_ILLOP)        // 6
TRAPHANDLER_NOEC(t_device, T_DEVICE)    // 7
TRAPHANDLER(t_dblflt, T_DBLFLT)            // 8
                                        // 9
TRAPHANDLER(t_tss, T_TSS)                // 10
TRAPHANDLER(t_segnp, T_SEGNP)            // 11
TRAPHANDLER(t_stack, T_STACK)            // 12
TRAPHANDLER(t_gpflt, T_GPFLT)            // 13
TRAPHANDLER(t_pgflt, T_PGFLT)            // 14
                                        // 15
TRAPHANDLER_NOEC(t_fperr, T_FPERR)        // 16
TRAPHANDLER(t_align, T_ALIGN)            // 17
TRAPHANDLER_NOEC(t_mchk, T_MCHK)        // 18
TRAPHANDLER_NOEC(t_simderr, T_SIMDERR)    // 19

TRAPHANDLER_NOEC(t_syscall, T_SYSCALL)

# IRQs
TRAPHANDLER_NOEC(irq_handler0, IRQ_OFFSET + 0)
TRAPHANDLER_NOEC(irq_handler1, IRQ_OFFSET + 1)
TRAPHANDLER_NOEC(irq_handler2, IRQ_OFFSET + 2)
TRAPHANDLER_NOEC(irq_handler3, IRQ_OFFSET + 3)
TRAPHANDLER_NOEC(irq_handler4, IRQ_OFFSET + 4)
TRAPHANDLER_NOEC(irq_handler5, IRQ_OFFSET + 5)
TRAPHANDLER_NOEC(irq_handler6, IRQ_OFFSET + 6)
TRAPHANDLER_NOEC(irq_handler7, IRQ_OFFSET + 7)
TRAPHANDLER_NOEC(irq_handler8, IRQ_OFFSET + 8)
TRAPHANDLER_NOEC(irq_handler9, IRQ_OFFSET + 9)
TRAPHANDLER_NOEC(irq_handler10, IRQ_OFFSET + 10)
TRAPHANDLER_NOEC(irq_handler11, IRQ_OFFSET + 11)
TRAPHANDLER_NOEC(irq_handler12, IRQ_OFFSET + 12)
TRAPHANDLER_NOEC(irq_handler13, IRQ_OFFSET + 13)
TRAPHANDLER_NOEC(irq_handler14, IRQ_OFFSET + 14)
TRAPHANDLER_NOEC(irq_handler15, IRQ_OFFSET + 15)

/*
 * code here for _alltraps
 */
 _alltraps:
	pushl %ds
	pushl %es
	pushal
	pushl $GD_KD
	popl %ds
	pushl $GD_KD
	popl %es
	pushl %esp 	// trapframe parameter
	call trap

