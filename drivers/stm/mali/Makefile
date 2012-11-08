#
# (C) Copyright 2010-2012 STMicroelectronics R&D Ltd
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

USING_TIMELINE_PROFILING_ENABLED := 0
USING_INTERNAL_TIMELINE_PROFILING_ENABLED := 0
USING_PROFILING_PRINT_L2_HITRATE_ON_GP_FINISH := 0
USING_PROFILING_SKIP_PP_AND_GP_JOBS := 0
USING_PROFILING_SKIP_PP_JOBS := 0
USING_MALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP_BETWEEN_APPS := 0
USING_MALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP := 0
USING_MALI_PP_SCHEDULER_KEEP_SUB_JOB_STARTS_ALIGNED := 0
USING_MALI_PMM_RUNTIME_JOB_CONTROL_ON := 0
USING_MALI_STATE_TRACKING := 1
OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB := 6

# Mali driver defines
ccflags-y += 	-DMALI_USE_UNIFIED_MEMORY_PROVIDER=0 \
		-DUSING_MALI400 \
		-DMALI_STATE_TRACKING=$(USING_MALI_STATE_TRACKING) \
		-DSVN_REV_STRING=\"$(SVN_REV)\" \
		-DMALI_PMM_RUNTIME_JOB_CONTROL_ON=$(USING_MALI_PMM_RUNTIME_JOB_CONTROL_ON) \
		-DMALI_TIMELINE_PROFILING_ENABLED=$(USING_TIMELINE_PROFILING_ENABLED) \
		-DMALI_INTERNAL_TIMELINE_PROFILING_ENABLED=$(USING_INTERNAL_TIMELINE_PROFILING_ENABLED) \
		-DPROFILING_PRINT_L2_HITRATE_ON_GP_FINISH=$(USING_PROFILING_PRINT_L2_HITRATE_ON_GP_FINISH) \
		-DPROFILING_SKIP_PP_AND_GP_JOBS=$(USING_PROFILING_SKIP_PP_AND_GP_JOBS) \
		-DPROFILING_SKIP_PP_JOBS=$(USING_PROFILING_SKIP_PP_JOBS) \
		-DMALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP_BETWEEN_APPS=$(USING_MALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP_BETWEEN_APPS) \
		-DMALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP=$(USING_MALI_PP_SCHEDULER_FORCE_NO_JOB_OVERLAP) \
		-DMALI_PP_SCHEDULER_KEEP_SUB_JOB_STARTS_ALIGNED=$(USING_MALI_PP_SCHEDULER_KEEP_SUB_JOB_STARTS_ALIGNED) \
		-DMALI_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB=$(OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB)

# Source files which always are included in a build
MALI_OBJS := $(addprefix linux/, \
	mali_dma_buf.o mali_kernel_linux.o mali_kernel_pm.o \
	mali_kernel_sysfs.o mali_osk_atomics.o mali_osk_irq.o \
	mali_osk_locks.o mali_osk_low_level_mem.o mali_osk_mali.o \
	mali_osk_math.o mali_osk_memory.o mali_osk_misc.o \
	mali_osk_notification.o mali_osk_pm.o mali_osk_time.o \
	mali_osk_timers.o mali_pmu_power_up_down.o mali_osk_wait_queue.o \
	mali_ukk_core.o mali_ukk_gp.o mali_ukk_mem.o \
	mali_ukk_pp.o mali_ukk_vsync.o )
MALI_OBJS += $(addprefix common/, \
	mali_block_allocator.o mali_cluster.o mali_device_pause_resume.o \
	mali_dlbu.o mali_gp.o mali_gp_job.o \
	mali_gp_scheduler.o mali_group.o mali_hw_core.o \
	mali_kernel_core.o mali_kernel_descriptor_mapping.o mali_kernel_mem_os.o \
	mali_kernel_memory_engine.o mali_kernel_vsync.o \
	mali_l2_cache.o mali_mem_validation.o mali_memory.o \
	mali_mmu.o mali_mmu_page_directory.o mali_pm.o \
	mali_pp.o mali_pmu.o mali_pp_job.o mali_pp_scheduler.o \
	mali_scheduler.o mali_session.o mali_user_settings_db.o )
MALI_OBJS += $(addprefix platform/default/, \
	mali_platform.o )

ifeq ($(USING_TIMELINE_PROFILING_ENABLED),1)
MALI_OBJS += $(addprefix linux/, \
	mali_osk_profiling_gator.o \
	mali_osk_profiling_internal.o \
	mali_ukk_profiling.o )
MALI_OBJS += $(addprefix common/, \
        mali_kernel_profiling.o)
endif

ifeq ($(CONFIG_MALI400_GPU_UTILIZATION),y)
MALI_OBJS += $(addprefix common/, \
        mali_kernel_utilization.o)
endif
 
ifeq ($(CONFIG_STM_MALI_DEBUG),y)
ccflags-y += -DDEBUG
endif

obj-$(CONFIG_STM_MALI) += mali.o
mali-objs := $(MALI_OBJS)
