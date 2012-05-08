obj-y				:= core.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_MACH_STM_FLI7610)	+= soc-fli7610.o
obj-$(CONFIG_MACH_STM_STIH415)	+= soc-stih415.o
obj-$(CONFIG_MACH_STM_FLI76XXHDK01)	+= board-fli76xxhdk01.o
obj-$(CONFIG_MACH_STM_B2000)	+= board-b2000.o
obj-$(CONFIG_MACH_STM_B2020)	+= board-b2020.o
obj-$(CONFIG_LOCAL_TIMERS)	+= localtimer.o
obj-$(CONFIG_PCI)		+= pci-glue.o

obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o

obj-$(CONFIG_PM)		+= pokeloop.o

# Suspend/MemSuspend
obj-$(CONFIG_SUSPEND)		+= suspend.o suspend-asm.o

ifdef CONFIG_STM_CPU_FREQ
obj-$(CONFIG_CPU_SUBTYPE_STIH415)	+= cpufreq.o
endif

# Hibernation on Disk
obj-$(CONFIG_HIBERNATION)	+= swsusp.o swsusp-asm.o

# Hibernation on memory
obj-$(CONFIG_HIBERNATION_ON_MEMORY)	+= hom.o hom-asm.o

obj-$(CONFIG_PM)		+= pm/
