/*
 *
 * Copyright (C) 2012 STMicroelectronics
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is under the terms of the
 * General Public License version 2 ONLY
 *
 * Generic CPUFreq driver able to work on several
 * ST Chips
 */
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/delay.h>	/* loops_per_jiffy */
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>

struct stm_cpufreq {
	struct mutex mutex;
	struct clk *cpu_clk;
	struct cpufreq_frequency_table *freq_table;
#ifdef CONFIG_PM
	unsigned long prev_cpu0_clk_rate;
#endif
#ifdef CONFIG_SMP
	/*
	 * Just one lpj_ref for all the CPUs is correct
	 * on STM platform becauae all the CPUs share
	 * the same clock
	 */
	unsigned long lpj_ref;
#endif
	unsigned long a9_perif_clk;
};

static struct stm_cpufreq *cpufreq;

/*
 * To avoid timining subsystem breakage (in the smp_gt.c)
 * STM-A9-CPUFreq manages only power of 2
 * divisor from 1 to 32
 */
#define NR_A9_DIVISOR	6


/*
 * Note:
 * - CPUFreq works in KHz
 * while
 * - Clk frmwork works in Hz
 *
 * therefore a conversion is required.
 */

#ifdef CONFIG_SMP

#include <linux/cpu.h>
#include <asm/cpu.h>

#endif

static int stm_cpufreq_target(struct cpufreq_policy *policy,
			     unsigned int target_freq, unsigned int relation)
{
	int i, idx = 0, ret = 0;
	cpumask_t cpus_allowed;
	struct cpufreq_freqs freqs = {
		.old = clk_get_rate(cpufreq->cpu_clk) / 1000,
		.flags = 0,
	};

	pr_debug("%s\n", __func__);

	if (!cpu_online(policy->cpu)) {
		ret = -ENODEV;
		goto _on_error;
	}

	if (cpufreq_frequency_table_target(policy,
				cpufreq->freq_table, target_freq,
				relation, &idx)) {
		ret = -EINVAL;
		goto _on_error;
	}
	if (!cpu_online(policy->cpu)) {
		pr_debug("%s: cpu not online\n", __func__);
		ret = -ENODEV;
		goto _on_error;
	}

	if (freqs.old == cpufreq->freq_table[idx].frequency)
		return 0;

	mutex_lock(&cpufreq->mutex);

	cpus_allowed = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(policy->cpu));
	BUG_ON(smp_processor_id() != policy->cpu);

	freqs.new = cpufreq->freq_table[idx].frequency;

	/* notifiers */
	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/*
	 * The following operations has to be managed
	 * as a big atomic operation
	 */

	cpufreq->a9_perif_clk = freqs.new * 500;

	/* 2.
	 * Clock scaling
	 */
	clk_set_rate(cpufreq->cpu_clk,
		cpufreq->freq_table[idx].frequency * 1000);

#ifdef CONFIG_SMP
	/*
	 * Note that loops_per_jiffy is not updated on SMP systems in
	 * cpufreq driver. So, update the per-CPU loops_per_jiffy value
	 * on frequency transition. We need to update all dependent CPUs.
	 */
	for_each_cpu(i, policy->cpus)
		per_cpu(cpu_data, i).loops_per_jiffy =
			cpufreq_scale(cpufreq->lpj_ref, freqs.old, freqs.new);

	loops_per_jiffy = cpufreq_scale(loops_per_jiffy, freqs.old, freqs.new);
#endif

	/* notifiers */
	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	set_cpus_allowed(current, cpus_allowed);

	mutex_unlock(&cpufreq->mutex);
_on_error:
	return ret;
}

static int stm_cpufreq_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, cpufreq->freq_table);
}

static unsigned int stm_cpufreq_get(unsigned int cpu)
{
	if (cpu >= NR_CPUS)
		return 0;

	return clk_get_rate(cpufreq->cpu_clk) / 1000;
}

static int stm_cpufreq_init(struct cpufreq_policy *policy)
{
	if (!cpu_online(policy->cpu))
		return -ENODEV;

	pr_debug("%s\n", __func__);
	/* cpuinfo and default policy values */
	policy->cur = clk_get_rate(cpufreq->cpu_clk) / 1000;
	policy->cpuinfo.transition_latency = 10 * 1000;

	cpufreq_frequency_table_cpuinfo(policy, cpufreq->freq_table);
	cpufreq_frequency_table_get_attr(cpufreq->freq_table, policy->cpu);
	/*
	 * STMicroelectronics-Cortex-A9 SMP shares clocks
	 * this has to be taken into account in case of CPUFreq support
	 */
#ifdef CONFIG_SMP
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_copy(policy->related_cpus, cpu_possible_mask);
#endif

	return 0;
}

static int stm_cpufreq_exit(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_cpuinfo(policy, cpufreq->freq_table);
}

#ifdef CONFIG_PM
static int stm_cpufreq_suspend(struct cpufreq_policy *policy)
{
	cpufreq->prev_cpu0_clk_rate = clk_get_rate(cpufreq->cpu_clk);
	return clk_set_rate(cpufreq->cpu_clk,
			    cpufreq->freq_table[0].frequency * 1000);
}

static int stm_cpufreq_resume(struct cpufreq_policy *policy)
{
	return clk_set_rate(cpufreq->cpu_clk, cpufreq->prev_cpu0_clk_rate);
}

#else
#define stm_cpufreq_suspend      NULL
#define stm_cpufreq_resume       NULL
#endif

static struct freq_attr *stm_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver stm_cpufreq_driver = {
	.owner = THIS_MODULE,
	.name = "stm-cpufreq",

	.init = stm_cpufreq_init,
	.exit = stm_cpufreq_exit,

	.verify = stm_cpufreq_verify,
	.get = stm_cpufreq_get,
	.target = stm_cpufreq_target,
	.attr = stm_cpufreq_attr,

	.suspend = stm_cpufreq_suspend,
	.resume = stm_cpufreq_resume,
	.flags = CPUFREQ_PM_NO_WARN,
};

static int __init stm_cpufreq_module_init(void)
{
	int idx, ret;
	unsigned long cpu_clk;

	pr_debug("%s\n", __func__);

	cpufreq = kmalloc(sizeof(*cpufreq), GFP_KERNEL);
	if (!cpufreq) {
		pr_err("[STM][CpuFreq] No memory available\n");
		ret = -ENOMEM;
		goto _err_0;
	}

	cpufreq->cpu_clk = clk_get(NULL, "cpu_clk");
	if (IS_ERR(cpufreq->cpu_clk)) {
		pr_err("[STM][CpuFreq] cpu_clk Not found\n");
		ret = -ENODEV;
		goto _err_1;
	}

	cpufreq->cpu_clk = clk_get_parent(cpufreq->cpu_clk);

	mutex_init(&cpufreq->mutex);

	cpufreq->freq_table = kcalloc(NR_A9_DIVISOR + 1,
		sizeof(struct cpufreq_frequency_table), GFP_KERNEL);
	if (!cpufreq->freq_table) {
		pr_err("[STM][CpuFreq]: Memory not available\n");
		ret = -ENOMEM;
		goto _err_2;
	}

	cpu_clk = clk_get_rate(cpufreq->cpu_clk) / 1000;
	for (idx = 0; idx < NR_A9_DIVISOR; ++idx) {
		cpufreq->freq_table[idx].index = idx;
		cpufreq->freq_table[idx].frequency = cpu_clk / (1 << idx);
		pr_debug("%s: Initialize idx %u @ %uKHz\n", __func__,
			idx, cpufreq->freq_table[idx].frequency);
	}
	/* mark the last entry */
	cpufreq->freq_table[NR_A9_DIVISOR].frequency = CPUFREQ_TABLE_END;

#ifdef CONFIG_SMP
	/*
	 * Set the initial condition for all the cpu.loop_per_jiffy
	 */
	cpufreq->lpj_ref = per_cpu(cpu_data, 0).loops_per_jiffy;
#endif
	ret = cpufreq_register_driver(&stm_cpufreq_driver);
	if (ret) {
		pr_err("stm: cpufreq: Error on registration\n");
		goto _err_3;
	}
	pr_info("stm: cpufreq registered\n");

	return 0;

_err_3:
	kfree(cpufreq->freq_table);
_err_2:
	clk_put(cpufreq->cpu_clk);
_err_1:
	kfree(cpufreq);
_err_0:
	cpufreq = NULL;
	return ret;
}

static void __exit stm_cpufreq_module_exit(void)
{
	if (!cpufreq)
		return;

	cpufreq_unregister_driver(&stm_cpufreq_driver);
	kfree(cpufreq->freq_table);
	clk_put(cpufreq->cpu_clk);
	kfree(cpufreq);
}

module_init(stm_cpufreq_module_init);
module_exit(stm_cpufreq_module_exit);

MODULE_DESCRIPTION("cpufreq driver for ST Platform");
MODULE_LICENSE("GPL");
