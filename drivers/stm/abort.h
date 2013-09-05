/* Spinlock for access to configuration space. We have to do read-modify-write
 * cycles here, so need to lock out for the duration to prevent races
 */
extern spinlock_t stm_abort_lock;

/*
 * On ARM platforms, we actually get a bus error returned when the PCIe IP
 * returns a UR or CRS instead of an OK. What we do to try to work around this
 * is hook the arm async abort exception and then check if the pc value is in
 * the region we expect bus errors could be generated. Fortunately we can
 * constrain the area the CPU will generate the async exception with the use of
 * a barrier instruction
 *
 * The stm_abort_flag is set if we see a bus error returned when we make config
 * requests.  It doesn't need to be an atomic variable, since it can only be
 * looked at safely in the regions protected by the spinlock anyway. However,
 * making it atomic avoids the need for volatile wierdness to prevent the
 * compiler from optimizing incorrectly
 */
extern atomic_t stm_abort_flag;

#ifdef CONFIG_ARM

/*
 * Macro to bung a label at a point in the code. I tried to do this with the
 * computed label extension of gcc (&&label), but it is too vulnerable to the
 * optimizer. The labels are forced to be word aligned to extend the range of
 * the branch instructions in thumb mode.
 */
#define EMIT_LABEL(label) \
	__asm__ __volatile__ ( \
			".align 2;\n" \
			#label":\n" \
			)
/* This vile macro gets the address of a label and puts it into a var */
#define GET_LABEL_ADDR(label, var) \
	__asm__ __volatile__ ( \
			"adr %0, " #label ";\n" \
			: "=r" (var) \
			)

/*
 * Holds the addresses where we are expecting an abort to be generated. We only
 * have to cope with one at a time as config read/write are spinlocked so
 * cannot be in the critical code section at the same time
 */
extern unsigned long abort_start, abort_end;

#else /* CONFIG_ARM */
#define EMIT_LABEL(label)
#define GET_LABEL_ADDR(label, var)
#endif

void stm_abort_init(void);
