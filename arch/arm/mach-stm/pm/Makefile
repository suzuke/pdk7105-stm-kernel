# STMicroelectronics
# ARM specific Power Management stuff

# Suspend
suspend-$(CONFIG_MACH_STM_STIG125)	:= suspend-stig125.o
suspend-$(CONFIG_CPU_SUBTYPE_STIH415)	:= suspend-stih415.o suspend-stxmpe41.o suspend-stxsasg1.o suspend-mcm.o
suspend-$(CONFIG_MACH_STM_STIH416)	:= suspend-stih416.o suspend-stxmpe42.o suspend-stxsasg2.o suspend-mcm.o
suspend-$(CONFIG_MACH_STM_FLI7610)	:= suspend-fli7610.o suspend-stxmpe41.o suspend-stxtae.o suspend-mcm.o
obj-$(CONFIG_SUSPEND)           += $(suspend-y)

# Hibernation on Memory
hom-$(CONFIG_CPU_SUBTYPE_STIH415)	:= hom-stih415.o
hom-$(CONFIG_MACH_STM_STIH416)		:= hom-stih416.o
hom-$(CONFIG_MACH_STM_FLI7610)		:= hom-fli7610.o
obj-$(CONFIG_HIBERNATION_ON_MEMORY)	+= $(hom-y)
