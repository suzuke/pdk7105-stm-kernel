#
# Makefile for the STMicroelectronics clocks.
#

clock-y					:= clock-common.o
clock-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= clock-fli7510.o
clock-$(CONFIG_CPU_SUBTYPE_FLI7610)	+= clock-fli7610.o clock-stxmpe41.o clock-stxtae.o
clock-$(CONFIG_CPU_SUBTYPE_STXH205)	+= clock-stxh205.o
clock-$(CONFIG_CPU_SUBTYPE_STIH415)	+= clock-stih415.o clock-stxmpe41.o clock-stxsasg1.o
clock-$(CONFIG_CPU_SUBTYPE_STX5197)	+= clock-stx5197.o
clock-$(CONFIG_CPU_SUBTYPE_STX5206)	+= clock-stx5206.o
clock-$(CONFIG_CPU_SUBTYPE_STX7105)	+= clock-stx7105.o
clock-$(CONFIG_CPU_SUBTYPE_STX7108)	+= clock-stx7108.o
clock-$(CONFIG_CPU_SUBTYPE_STX7141)	+= clock-stx7141.o

obj-y	+= $(clock-y)
