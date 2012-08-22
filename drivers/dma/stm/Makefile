ccflags-$(CONFIG_DMADEVICES_DEBUG)  := -DDEBUG
ccflags-$(CONFIG_DMADEVICES_VDEBUG) += -DVERBOSE_DEBUG

obj-$(CONFIG_STM_FDMA) += stm-fdma.o

stm-fdma-$(CONFIG_STM_FDMA_AUDIO) += stm_fdma_audio.o
stm-fdma-$(CONFIG_STM_FDMA_MCHI) += stm_fdma_mchi.o
stm-fdma-$(CONFIG_STM_FDMA_TELSS) += stm_fdma_telss.o
stm-fdma-$(CONFIG_DEBUG_FS) += stm_fdma_debugfs.o

stm-fdma-objs:= stm_fdma.o \
		stm_fdma_desc.o \
		stm_fdma_dreq.o \
		stm_fdma_fw.o \
		stm_fdma_hw.o \
		stm_fdma_xbar.o	\
		$(stm-fdma-y)
