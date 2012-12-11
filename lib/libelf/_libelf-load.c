/*
 * File     : lib/libelf/_libelf-load.c
 * Synopsis : Physical address ELF loader
 * Author   : David Cook <david.cook@st.com>
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 */

#ifdef CONFIG_ARM
#include <asm/setup.h>
#endif /* CONFIG_ARM */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>


/* Function which returns non-zero if the first range entirely contains the
 * second.
 * Args: start range 1 (inclusive)
 *	 end range 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 end range 2 (exclusive)
 */
static inline int rangeContainsA(uint32_t s1, uint32_t e1,
				 uint32_t s2, uint32_t e2)
{
	if (s2 >= s1 && e2 <= e1)
		return 1;
	return 0;
}


/* Function which returns non-zero if the first range entirely contains the
 * second.
 * Args: start range 1 (inclusive)
 *	 range length 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 range length 2 (exclusive)
 */
static inline int rangeContainsL(uint32_t s1, uint32_t l1,
				 uint32_t s2, uint32_t l2)
{
	if (s2 >= s1 && s2 + l2 <= s1 + l1)
		return 1;
	return 0;
}


#ifdef CONFIG_ARM
/* Function which returns non-zero if the 2 ranges specified overlap each other.
 * Args: start range 1 (inclusive)
 *	 range length 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 range length 2 (exclusive)
 */
static int overlapping(uint32_t s1, uint32_t l1, uint32_t s2, uint32_t l2)
{
	if (s1 + l1 <= s2)
		return 0;
	if (s2 + l2 <= s1)
		return 0;
	return 1;
}
#endif /* CONFIG_ARM */


/* Function which checks the physical destination addresses are within any
 * allowed ranges for this load (as specified in the loadParams), or if no
 * allowed ranges were specified, that all ranges to load are outside of kernel
 * memory.
 * Returns 0 for allowed, non-zero for not allowed.
 */
static int allowedRangeCheck(const ElfW(Phdr) *phdr, const uint32_t segNum,
			     const struct ELFW(LoadParams) *loadParams)
{
	if (loadParams && loadParams->numAllowedRanges) {
		int i, inLimits = 0;
		for (i = 0 ; i < loadParams->numAllowedRanges; i++) {
			struct ELFW(MemRange) *allowed =
					&loadParams->allowedRanges[i];
			if (rangeContainsA(allowed->base, allowed->top,
					   phdr->p_paddr,
					   phdr->p_paddr + phdr->p_memsz)) {
				inLimits = 1;
				break;
			}
		}
		if (!inLimits) {
			pr_err("libelf: Segment %d outside allowed limits\n",
			       segNum);
			return -EINVAL;
		}
	} else {
		/* If no limits were specified, ensure the segment
		 * is outside of kernel memory.
		 * We only check on some architectures.  We allow the load with
		 * a warning for architectures we have not checked on.
		 */
#ifdef CONFIG_SUPERH
		/* This was the check used by the old 'coprocessor' driver which
		 * has been removed (except it allowed the ST40 .empty_zero_page
		 * too and we don't bother...see coproc_check_area() in the
		 * STLinux 2.4 0211 kernel file drivers/stm/copro-st_socs.c to
		 * add that).
		 */
		unsigned long startPFN, endPFN;
		startPFN = PFN_DOWN(phdr->p_paddr);
		endPFN = PFN_DOWN(phdr->p_paddr + phdr->p_memsz);
		if (startPFN < max_low_pfn && endPFN > min_low_pfn) {
#elif defined(CONFIG_ARM)
		/* The ARM supports multiple physical memory ranges for the
		 * kernel.
		 */
		int i;
		int overlap = 0;
		for (i = 0; i < meminfo.nr_banks; i++) {
			if (overlapping(phdr->p_paddr, phdr->p_memsz,
					meminfo.bank[i].start,
					meminfo.bank[i].size)) {
				overlap = 1;
				break;
			}
		}
		if (overlap) {
#else /* Other architectures */
		static int warned;	/* Zero-initialised as static */
		if (!warned) {
			pr_warning("libelf: Not checking for kernel memory clash during ELF segment loading\n");
			warned++;
		}
		if (0) {
#endif /* Architectures */
			pr_err("libelf: Segment %d overlaps kernel memory\n",
			       segNum);
			return -EINVAL;
		}
	}
	return 0;
}


/* Function which returns an IO address from an existing IO mapping (as
 * provided in the loadParams) if the entire segment can be loaded via that
 * mapping.
 * Returns NULL if there is no existing mapping to use.
 */
static void __iomem *findIOMapping(const ElfW(Addr) pAddr,
				   const unsigned long size,
				   const struct ELFW(LoadParams) *loadParams)
{
	int i;

	if (!loadParams || loadParams->numExistingMappings == 0)
		return NULL;

	for (i = 0; i < loadParams->numExistingMappings; i++) {
		struct ELFW(IORemapMapping) *exMap =
				&loadParams->existingMappings[i];
		if (rangeContainsL(exMap->physBase, exMap->size,
				   pAddr, size)) {
			return exMap->vIOBase + (pAddr - exMap->physBase);
		}
	}
	return NULL;
}


/* Perform an ELF load to physical addresses, returning non-zero on failure,
 * else 0.
 * loadParams specifies checks to perform and restrictions on the load.
 * For successful loads:
 *  - the ELF entry address is passed back via entryAddr
 */
int ELFW(physLoad)(const struct ELFW(info) *elfInfo,
		   const struct ELFW(LoadParams) *loadParams,
		   uint32_t *entryAddr)
{
	ElfW(Phdr)	*phdr = elfInfo->progbase;
	void		*elfBase = elfInfo->base;
	unsigned int	loadedSegNum = 0;
	int		i;

	/* Normal loading */
	for (i = 0; i < elfInfo->header->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD) {
			unsigned long memSize, copySize, setSize;
			void __iomem *ioDestAddr;
			void *virtSrcAddr;
			bool newIOMapping = true;
			int res;

			memSize = phdr[i].p_memsz;

			/* Some ST200 Micro Toolset ELF files have a strange 0
			 * size segment as a result of a .note section.
			 */
			if (memSize == 0)
				continue;

			/* Check load range limits (from loadParams) */
			res = allowedRangeCheck(&phdr[i], i, loadParams);
			if (res)
				return res;

			{
				virtSrcAddr = elfBase + phdr[i].p_offset;
				copySize = phdr[i].p_filesz;
				setSize = memSize - phdr[i].p_filesz;
			}

			/* Can we use an existing mapping as provided by
			 * loadParams?
			 */
			if (loadParams->numExistingMappings) {
				ioDestAddr = findIOMapping(phdr[i].p_paddr,
							   memSize,
							   loadParams);
				if (ioDestAddr)
					newIOMapping = false;
			}

			if (newIOMapping) {
				/* Note: On ARMv6 or higher multiple mappings
				 * with different attributes are not allowed
				 * so this isn't good enough if a mapping
				 * already exists and we just were not told -
				 * drivers calling us must have recorded the
				 * existing mappings in the load parameters so
				 * this path is not used!
				 */
				ioDestAddr = ioremap(phdr[i].p_paddr, memSize);
				if (ioDestAddr == NULL) {
					if (virtSrcAddr !=
					    elfBase + phdr[i].p_offset)
						kfree(virtSrcAddr);
					pr_err("libelf: Segment %d ioremap of 0x%08x failed\n",
					       i,
					       (unsigned int)phdr[i].p_paddr);
					return -ENOMEM;
				}
			}
			memcpy_toio(ioDestAddr, virtSrcAddr, copySize);
			if (setSize)
				memset_io(ioDestAddr + copySize, 0, setSize);

			if (loadParams && loadParams->ptLoadCallback)
				loadParams->ptLoadCallback(
					loadedSegNum, loadParams->privData,
					phdr[i].p_paddr, virtSrcAddr, copySize,
					memSize);

			if (virtSrcAddr != elfBase + phdr[i].p_offset)
				kfree(virtSrcAddr);

			if (newIOMapping)
				iounmap(ioDestAddr);
			loadedSegNum++;
		}
	}

	if (entryAddr)
		*entryAddr = elfInfo->header->e_entry;
	return 0;
}
EXPORT_SYMBOL(ELFW(physLoad));
