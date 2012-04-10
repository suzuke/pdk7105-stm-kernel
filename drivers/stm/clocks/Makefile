#
# Makefile for the STMicroelectronics clocks.
#

clock-y					:= clock-common.o
clock-$(CONFIG_CPU_SUBTYPE_FLI7610)	+= clock-fli7610.o clock-stxmpe41.o clock-stxtae.o
clock-$(CONFIG_CPU_SUBTYPE_STXH205)	+= clock-stxh205.o
clock-$(CONFIG_CPU_SUBTYPE_STIH415)	+= clock-stih415.o clock-stxmpe41.o clock-stxsasg1.o
clock-$(CONFIG_CPU_SUBTYPE_STX7108)	+= clock-stx7108.o

obj-y	+= $(clock-y)
