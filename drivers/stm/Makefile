#
# Makefile for the STMicroelectronics device drivers.
#

obj-y					+= emi.o
obj-y					+= gpio.o
obj-y					+= pad.o
obj-y					+= sysconf.o
obj-y					+= device.o
obj-y					+= clk.o
obj-y					+= clocks/

obj-$(CONFIG_STM_SH_DMA_API)		+= sh-dma-api.o sh-dma-sysfs.o
obj-$(CONFIG_STM_DMA)			+= fdma.o fdma-xbar.o
obj-$(CONFIG_STM_MIPHY)			+= miphy.o
obj-$(CONFIG_STM_MIPHY_TAP)		+= miphy_tap.o tap.o
obj-$(CONFIG_STM_MIPHY_PCIE_MP)		+= miphy_pcie_mp.o
obj-$(CONFIG_STM_MIPHY_DUMMY)		+= miphy_dummy.o
obj-$(CONFIG_STM_COPROCESSOR_CLASS)	+= coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_ST40)	+= coproc-st40.o
obj-$(CONFIG_STM_COPROCESSOR_ST200)	+= coproc-st200.o
obj-$(CONFIG_STM_RNG)			+= rng.o
obj-$(CONFIG_STM_MALI)			+= mali/
obj-$(CONFIG_STM_AMBA_BRIDGE)		+= amba_bridge.o

obj-$(CONFIG_PM)			+= wakeup_devices.o
obj-$(CONFIG_PM)			+= pm_notify.o
obj-$(CONFIG_PMS)			+= pms.o
obj-$(CONFIG_PM_RUNTIME)		+= pm_runtime.o

obj-$(CONFIG_STM_PCI_EMISS)		+= pci-emiss.o
obj-$(CONFIG_STM_PCIE)			+= pcie.o

obj-$(CONFIG_CPU_SUBTYPE_FLI7510)	+= fli7510.o fli7510_clock.o fli7510_audio.o fli7510_comms.o fli7510_hispeed.o fli7510_pci.o
obj-$(CONFIG_CPU_SUBTYPE_FLI7610)	+= fli7610.o fli7610_clock.o fli7610_audio.o pio-control.o \
					   fli7610_usb.o fli7610_comms.o mpe41_devices.o
obj-$(CONFIG_CPU_SUBTYPE_STXH205)	+= stxh205.o stxh205_clock.o stxh205_comms.o stxh205_hispeed.o pio-control.o
obj-$(CONFIG_CPU_SUBTYPE_STIH415)	+= stih415.o stih415_clock.o stih415_audio.o stih415_hispeed.o stih415_usb.o \
					   stih415_comms.o stih415_pci.o pio-control.o mpe41_devices.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= stx7105.o stx7105_clock.o stx7105_audio.o stx7105_comms.o stx7105_hispeed.o stx7105_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7108)	+= stx7108.o stx7108_clock.o stx7108_audio.o stx7108_comms.o stx7108_hispeed.o stx7108_pci.o pio-control.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= stx7141.o stx7141_clock.o stx7141_audio.o stx7141_comms.o stx7141_hispeed.o
