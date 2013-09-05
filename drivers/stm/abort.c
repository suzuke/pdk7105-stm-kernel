#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/signal.h>

DEFINE_SPINLOCK(stm_abort_lock);
atomic_t stm_abort_flag;

#ifdef CONFIG_ARM

unsigned long abort_start, abort_end;

static int stm_abort_handler(unsigned long addr, unsigned int fsr,
			     struct pt_regs *regs)
{
	unsigned long pc = regs->ARM_pc;

	/*
	 * If it isn't the expected place, then return 1 which will then fall
	 * through to the default error handler. This means that if we get a
	 * bus error for something other than PCIE config read/write accesses
	 * we will not just carry on silently.
	 */
	if (pc < abort_start || pc >= abort_end)
		return 1;

	/* Again, if it isn't an async abort then return to default handler */
	if (!((fsr & (1 << 10)) && ((fsr & 0xf) == 0x6)))
		return 1;

	/* Restart after exception address */
	regs->ARM_pc += 4;

	/* Set abort flag */
	atomic_set(&stm_abort_flag, 1) ;

	/* Barrier to ensure propogation */
	mb();

	return 0;
}

#endif

void stm_abort_init(void)
{
#ifdef CONFIG_ARM
	/*
	 * We have to hook the abort handler so that we can intercept bus
	 * errors when doing config read/write that return UR, which is flagged
	 * up as a bus error
	 */
	hook_fault_code(16+6, stm_abort_handler, SIGBUS, 0,
			"imprecise external abort");
#endif
}
