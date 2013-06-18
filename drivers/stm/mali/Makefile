#
# (C) Copyright 2010-2013 STMicroelectronics R&D Ltd
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#
MALI_DIR := $(srctree)/drivers/stm/mali

# Get ARM release version string
SVN_REV := $(shell [ -e $(MALI_DIR)/version ] && cat $(MALI_DIR)/version)

# Driver include paths
ccflags-y := -I$(MALI_DIR) -I$(MALI_DIR)/include -I$(MALI_DIR)/common -I$(MALI_DIR)/linux -I$(MALI_DIR)/platform 
ccflags-y += -I$(MALI_DIR)/linux/license/gpl

# STM Mali defaults configuration
STM_BUFFER_SIZE_IN_MB ?= 16
STM_GPU_UTILIZATION := 0
STM_PROFILING_SKIP_PP_JOBS ?= 0
STM_PROFILING_SKIP_PP_AND_GP_JOBS ?= 0
STM_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP ?= 0
STM_PP_SCHEDULER_KEEP_SUB_JOB_STARTS_ALIGNED ?= 0
STM_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP_BETWEEN_APPS ?= 0

# Check configurable defines
ifneq ($(CONFIG_STM_MALI_PROFILING),)
ifndef CONFIG_TRACEPOINTS
$(warning "CONFIG_TRACEPOINTS required for Mali profiling. Force disable.")
CONFIG_STM_MALI_PROFILING := 0
else
ccflags-y +=	-DCONFIG_MALI400_PROFILING
ifneq ($(CONFIG_STM_MALI_INTERNAL_PROFILING),)
ccflags-y +=	-DCONFIG_MALI400_INTERNAL_PROFILING
endif
endif
endif

# Mali driver setup
ccflags-y +=	-DUSING_GPU_UTILIZATION=$(STM_GPU_UTILIZATION) \
		-DSVN_REV_STRING=\"$(SVN_REV)\" \
		-DMALI_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB=$(STM_BUFFER_SIZE_IN_MB) \
		-DPROFILING_SKIP_PP_JOBS=$(STM_PROFILING_SKIP_PP_JOBS) \
		-DPROFILING_SKIP_PP_AND_GP_JOBS=$(STM_PROFILING_SKIP_PP_AND_GP_JOBS) \
		-DMALI_STATE_TRACKING=1 \
		-DMALI_UPPER_HALF_SCHEDULING \
		-DCONFIG_MALI_DMA_BUF_MAP_ON_ATTACH

# Source files which always are included in a build
mali-y += \
        linux/mali_osk_atomics.o \
        linux/mali_osk_irq.o \
        linux/mali_osk_wq.o \
        linux/mali_osk_locks.o \
        linux/mali_osk_wait_queue.o \
        linux/mali_osk_low_level_mem.o \
        linux/mali_osk_math.o \
        linux/mali_osk_memory.o \
        linux/mali_osk_misc.o \
        linux/mali_osk_mali.o \
        linux/mali_osk_notification.o \
        linux/mali_osk_time.o \
        linux/mali_osk_timers.o

mali-y += \
        linux/mali_ukk_mem.o \
        linux/mali_ukk_gp.o \
        linux/mali_ukk_pp.o \
        linux/mali_ukk_core.o

mali-y += \
        common/mali_kernel_core.o \
        linux/mali_kernel_linux.o \
        common/mali_kernel_descriptor_mapping.o \
        common/mali_session.o \
        common/mali_device_pause_resume.o \
        common/mali_kernel_vsync.o \
        linux/mali_ukk_vsync.o \
        linux/mali_kernel_sysfs.o \
        common/mali_mmu.o \
        common/mali_mmu_page_directory.o \
        common/mali_memory.o \
        common/mali_kernel_memory_engine.o \
        common/mali_block_allocator.o \
        common/mali_kernel_mem_os.o \
        common/mali_mem_validation.o \
        common/mali_hw_core.o \
        common/mali_gp.o \
        common/mali_pp.o \
        common/mali_pp_job.o \
        common/mali_gp_job.o \
        common/mali_scheduler.o \
        common/mali_gp_scheduler.o \
        common/mali_pp_scheduler.o \
        common/mali_group.o \
        common/mali_dlbu.o \
        common/mali_broadcast.o \
        common/mali_pm.o \
        common/mali_pmu.o \
        common/mali_user_settings_db.o \
        common/mali_kernel_utilization.o \
        common/mali_l2_cache.o \
        linux/mali_osk_pm.o \
        linux/mali_pmu_power_up_down.o

mali-$(CONFIG_STM_MALI_PROFILING) += \
	linux/mali_ukk_profiling.o \
	linux/mali_osk_profiling.o

mali-$(CONFIG_STM_MALI_INTERNAL_PROFILING) += \
	linux/mali_profiling_internal.o \
	timestamp-default/mali_timestamp.o
ccflags-$(CONFIG_STM_MALI_INTERNAL_PROFILING) += \
	-I$(MALI_DIR)/timestamp-default

mali-$(CONFIG_STM_MALI_DMA_SHARED_BUFFER) += \
	linux/mali_dma_buf.o

mali-$(CONFIG_STM_MALI_SYNC) += \
	linux/mali_sync.o \
	linux/mali_sync_user.o

ccflags-$(CONFIG_STM_MALI_DEBUG) += -DDEBUG

obj-$(CONFIG_STM_MALI) += mali.o
