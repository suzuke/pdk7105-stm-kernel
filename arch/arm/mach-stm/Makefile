obj-y				:= core.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_OF)		+= common-dt.o
obj-$(CONFIG_MACH_STM_FLI7610)	+= soc-fli7610.o mpe41.o
obj-$(CONFIG_MACH_STM_STIG125)	+= soc-stig125.o
obj-$(CONFIG_MACH_STM_STIH415)	+= soc-stih415.o mpe41.o
obj-$(CONFIG_MACH_STM_STIH416)	+= soc-stih416.o mpe42.o
obj-$(CONFIG_MACH_STM_FLI76XXHDK01)	+= board-fli76xxhdk01.o
ifdef CONFIG_MACH_STM_B2000
obj-$(CONFIG_MACH_STM_STIH415)	+= board-b2000.o
obj-$(CONFIG_MACH_STM_STIH416)	+= board-b2000_h416.o
endif
ifdef CONFIG_MACH_STM_B2020
obj-$(CONFIG_MACH_STM_STIH415)	+= board-b2020.o
obj-$(CONFIG_MACH_STM_STIH416)	+= board-b2020_h416.o
endif
obj-$(CONFIG_MACH_STM_STIG125_BOARD)	+= board-stig125-dt.o
obj-$(CONFIG_MACH_STM_STIH415_BOARD)	+= board-stih415-dt.o
obj-$(CONFIG_MACH_STM_STIH416_BOARD)	+= board-stih416-dt.o
obj-$(CONFIG_MACH_STM_B2044)	+= board-b2044.o
obj-$(CONFIG_MACH_STM_B2078)	+= board-b2078.o
obj-$(CONFIG_MACH_STM_B2092)	+= board-b2092.o
obj-$(CONFIG_CACHE_L2X0)	+= l2x0.o
obj-$(CONFIG_PCI)		+= pci-glue.o

obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o

obj-$(CONFIG_PM)		+= pokeloop.o

# Suspend/MemSuspend
obj-$(CONFIG_SUSPEND)		+= suspend.o suspend-asm.o

ifdef CONFIG_STM_CPU_FREQ
obj-$(CONFIG_CPU_SUBTYPE_STIH415)	+= cpufreq.o
obj-$(CONFIG_MACH_STM_STIH416)	+= cpufreq.o
endif

# Hibernation on Disk
obj-$(CONFIG_HIBERNATION)	+= swsusp.o swsusp-asm.o

# Hibernation on memory
obj-$(CONFIG_HIBERNATION_ON_MEMORY)	+= hom.o hom-asm.o

obj-$(CONFIG_PM)		+= pm/
