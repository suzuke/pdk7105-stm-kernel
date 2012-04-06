obj-y				:= core.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_MACH_STM_FLI7610)	+= soc-fli7610.o
obj-$(CONFIG_MACH_STM_STIH415)	+= soc-stih415.o
obj-$(CONFIG_MACH_STM_FLI76XXHDK01)	+= board-fli76xxhdk01.o
obj-$(CONFIG_MACH_STM_B2000)	+= board-b2000.o
obj-$(CONFIG_LOCAL_TIMERS)	+= localtimer.o
obj-$(CONFIG_PCI)		+= pci-glue.o

obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o

# Hibernation on Disk
obj-$(CONFIG_HIBERNATION)	+= swsusp.o swsusp-asm.o

