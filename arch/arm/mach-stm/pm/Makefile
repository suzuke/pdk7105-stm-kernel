# STMicroelectronics
# ARM specific Power Management stuff

# Suspend
suspend-${CONFIG_CPU_SUBTYPE_STIH415}	:= suspend-stih415.o suspend-stxmpe41.o suspend-stxsasg1.o
obj-$(CONFIG_SUSPEND)           += $(suspend-y)

# Hibernation on Memory
hom-$(CONFIG_CPU_SUBTYPE_STIH415)	:= hom-stih415.o
obj-$(CONFIG_HIBERNATION_ON_MEMORY)	+= $(hom-y)
