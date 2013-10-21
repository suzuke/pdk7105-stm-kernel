/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/stm/device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/err.h>

struct stm_device_state {
	struct device *dev;
	struct stm_device_config *config;
	enum stm_device_power_state power_state;
	struct stm_pad_state *pad_state;
	struct sysconf_field *sysconf_fields[0]; /* To be expanded */
};

static int __stm_device_init(struct stm_device_state *state,
		struct stm_device_config *config, struct device *dev)
{
	int i;

	dev_dbg(dev, "%s, dev: %s\n", __func__, dev_name(dev));

	state->dev = dev;
	state->config = config;
	state->power_state = stm_device_power_off;

	for (i = 0; i < config->sysconfs_num; i++) {
		struct stm_device_sysconf *sysconf = &config->sysconfs[i];

		dev_dbg(dev, "%s, claim sysconfs[%d]\n",
				__func__, i);

		state->sysconf_fields[i] = sysconf_claim(sysconf->regtype,
				sysconf->regnum, sysconf->lsb, sysconf->msb,
				dev_name(dev));
		if (!state->sysconf_fields[i])
			goto sysconf_error;
	}

	if (config->init) {
		dev_dbg(dev, "%s, config->init\n", __func__);
		if (config->init(state))
			goto sysconf_error;
	}

	if (config->pad_config &&
	    (state->pad_state = stm_pad_claim(config->pad_config,
					      dev_name(dev))) == NULL)
		goto pad_error;

	stm_device_power(state, stm_device_power_on);

	return 0;

pad_error:
	if (config->exit)
		config->exit(state);

sysconf_error:
	for (i--; i>=0; i--)
		sysconf_release(state->sysconf_fields[i]);
	dev_err(dev, "%s, sysconf error\n", __func__);
	return -EBUSY;
}

static void __stm_device_exit(struct stm_device_state *state)
{
	struct stm_device_config *config = state->config;
	int i;

	stm_device_power(state, stm_device_power_off);

	if (config->pad_config)
		stm_pad_release(state->pad_state);

	if (config->exit)
		config->exit(state);

	for (i = 0; i < config->sysconfs_num; i++)
		sysconf_release(state->sysconf_fields[i]);
}

struct stm_device_state *stm_device_init(struct stm_device_config *config,
		struct device *dev)
{
	struct stm_device_state *state;

	BUG_ON(!dev);
	dev_dbg(dev, "%s\n", __func__);

	BUG_ON(!config);

	state = kzalloc(sizeof(*state) +
		sizeof(*state->sysconf_fields) * config->sysconfs_num,
		GFP_KERNEL);

	if (state && __stm_device_init(state, config, dev) != 0)
		state = NULL;

	return state;
}
EXPORT_SYMBOL(stm_device_init);

void stm_device_exit(struct stm_device_state *state)
{
	BUG_ON(!state);

	__stm_device_exit(state);

	kfree(state);
}
EXPORT_SYMBOL(stm_device_exit);



static void stm_device_devres_exit(struct device *dev, void *res)
{
	struct stm_device_state *state = res;

	BUG_ON(!state);

	__stm_device_exit(state);
}

static int stm_device_devres_match(struct device *dev, void *res, void *data)
{
	struct stm_device_state *state = res, *match = data;

	return state == match;
}

struct stm_device_state *devm_stm_device_init(struct device *dev,
	struct stm_device_config *config)
{
	struct stm_device_state *state;

	BUG_ON(!dev);
	BUG_ON(!config);

	state = devres_alloc(stm_device_devres_exit,
			sizeof(*state) + sizeof(*state->sysconf_fields) *
			config->sysconfs_num, GFP_KERNEL);

	if (state) {
		if (__stm_device_init(state, config, dev) == 0) {
			devres_add(dev, state);
		} else {
			devres_free(state);
			state = NULL;
		}
	}

	return state;
}
EXPORT_SYMBOL(devm_stm_device_init);

void devm_stm_device_exit(struct device *dev, struct stm_device_state *state)
{
	int err;

	__stm_device_exit(state);

	err = devres_destroy(dev, stm_device_devres_exit,
			stm_device_devres_match, state);
	WARN_ON(err);
}
EXPORT_SYMBOL(devm_stm_device_exit);



static int stm_device_find_sysconf(struct stm_device_config *config,
		const char *name)
{
	int result = -1;
	int i;

	BUG_ON(!name);

	for (i = 0; i < config->sysconfs_num; i++) {
		struct stm_device_sysconf *pad_sysconf = &config->sysconfs[i];

		if (strcmp(name, pad_sysconf->name) == 0) {
			result = i;
			break;
		}
	}

	return result;
}

void stm_device_sysconf_write(struct stm_device_state *state,
		const char* name, unsigned long long value)
{
	int i;
	dev_dbg(state->dev, "%s write %lld to sysconf %s\n",
			__func__, value, name);

	i = stm_device_find_sysconf(state->config, name);
	if (i >= 0)
		sysconf_write(state->sysconf_fields[i], value);
	else
		dev_err(state->dev, "failed to write on SYSCFG bit %s\n", name);
}
EXPORT_SYMBOL(stm_device_sysconf_write);

unsigned long long stm_device_sysconf_read(struct stm_device_state *state,
		const char* name)
{
	int i;

	i = stm_device_find_sysconf(state->config, name);
	if (i >= 0)
		return sysconf_read(state->sysconf_fields[i]);
	else
		dev_err(state->dev, "failed to err on SYSCFG bit %s\n", name);

	return -EPERM;
}
EXPORT_SYMBOL(stm_device_sysconf_read);

void stm_device_setup(struct stm_device_state *device_state)
{
	if (device_state->config &&
	    device_state->config->pad_config)
		stm_pad_setup(device_state->pad_state);

	if (device_state->config->init)
		device_state->config->init(device_state);
}
EXPORT_SYMBOL(stm_device_setup);

void stm_device_power(struct stm_device_state *device_state,
		enum stm_device_power_state power_state)
{
	if (device_state->power_state == power_state)
		return;

	if (device_state->config->power)
		device_state->config->power(device_state, power_state);

	device_state->power_state = power_state;
}
EXPORT_SYMBOL(stm_device_power);

struct stm_pad_state* stm_device_get_pad_state(struct stm_device_state *state)
{
	return state->pad_state;
}
EXPORT_SYMBOL(stm_device_get_pad_state);

struct stm_device_config* stm_device_get_config(struct stm_device_state *state)
{
	return state->config;
}
EXPORT_SYMBOL(stm_device_get_config);

struct device *stm_device_get_dev(struct stm_device_state *state)
{
	return state->dev;
}
EXPORT_SYMBOL(stm_device_get_dev);

#ifdef CONFIG_OF

#define STM_OF_STM_DEV_SYCONF_CELLS	(4)
#define MAX_FEEBACK_RETRY	(100)

int stm_of_run_clk_seq(struct stm_device_state *state,
			struct device_node *child)
{
	struct clk *clk = NULL, *pclk = NULL;
	int ret = -EINVAL;
	bool disable;
	struct device *dev = state->dev;
	u32 rate = 0, prate = 0;
	const char *clk_name = NULL, *pclk_name = NULL;

	ret = of_property_read_string(child, "clk-name", &clk_name);
	if (ret) {
		dev_err(dev, "Invalid clk sequence\n");
		return ret;
	}

	of_property_read_u32(child, "clk-rate", &rate);
	disable = of_property_read_bool(child, "clk-disable");

	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		dev_err(dev, "clk [%s] not found\n", clk_name);
		return PTR_ERR(clk);
	}

	/* Check if we need reparenting */
	ret = of_property_read_string(child, "clk-parent-name", &pclk_name);
	if (!ret) {
		of_property_read_u32(child, "clk-parent-rate", &prate);

		pclk = clk_get(NULL, pclk_name);
		if (IS_ERR(pclk)) {
			dev_err(dev, "clk [%s] not found\n", pclk_name);
			ret = PTR_ERR(pclk);
			goto clk_err;
		}

		if (prate)
			clk_set_rate(pclk, prate);

		ret = clk_set_parent(clk, pclk);
		if (ret)
			goto pclk_err;
	}

	if (disable) {
		clk_disable_unprepare(clk);
		clk_put(clk);
		return 0;
	}

	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(dev, "enable clk [%s] failed\n", clk_name);
		goto pclk_err;
	}

	if (rate)
		clk_set_rate(clk, rate);

	return 0;

pclk_err:
	if (pclk)
		clk_disable_unprepare(pclk);
pclk_en_err:
	if (pclk)
		clk_put(pclk);
clk_err:
	clk_put(clk);

	return ret;

}

int stm_of_run_seq(struct stm_device_state *state, struct device_node *seq)
{
	struct device_node *child = NULL;
	struct property *pp;
	char step[10];
	int num_steps = 0, i, k;

	dev_dbg(state->dev, "%s\n", __func__);

	if (!seq)
		return 0;

	child = of_get_next_child(seq, NULL);
	for ( ; child != NULL; ) {
		child = of_get_next_child(seq, child);
		num_steps++;
	}

	for (i = 0; i < num_steps; i++) {
		const char *name = NULL;
		const char *type = NULL;
		int val = 0;

		sprintf(step, "step%d", i);
		child = of_get_child_by_name(seq, step);
		if (!child)
			continue;

		pp = of_find_property(child, "type", NULL);
		if (!pp)
			continue;
		type = pp->value;

		if (!strcmp(type, "sysconf")) {
			for_each_property_of_node(child, pp) {
				if (!strcmp(pp->name, "type")) {
					continue;
				} else {
					if (pp->length != sizeof(u32))
						continue;
					name = pp->name;
					val = be32_to_cpup(pp->value);
					dev_dbg(state->dev,
						"%s write sysconf %s (i=%d)\n",
						__func__, name, i);
					stm_device_sysconf_write(state
								, name, val);
				}
			}
		}
		if (!strcmp(type, "sysconf-feedback")) {
			for_each_property_of_node(child, pp) {
				if (!strcmp(pp->name, "type")) {
					continue;
				} else {
					if (pp->length != sizeof(u32))
						continue;
					name = pp->name;
					val = be32_to_cpup(pp->value);
				}
				for (k = 0; k++ < MAX_FEEBACK_RETRY;) {
					if (stm_device_sysconf_read(state, name)
						== val)
						break;
					schedule();
				}
			}
		}

		if (!strcmp(type, "clock")) {
			if (stm_of_run_clk_seq(state, child))
				dev_err(state->dev, "Failed to run [%s]\n",
					 step);
		}

	}
	return 0;
}

int stm_of_run_device_seq(struct stm_device_state *state, char *seq_name)
{
	struct device *dev = stm_device_get_dev(state);
	struct device_node *np = dev->of_node;
	struct device_node *devnode, *seqs, *seqnode;

	devnode = of_parse_phandle(np, "device-config", 0);
	if (devnode) {
		seqs = of_parse_phandle(devnode, "device-seqs", 0);
		if (seqs)
			seqnode = of_get_child_by_name(seqs,  seq_name);
		else {
			dev_err(dev, "%s: cannot find seqnode\n", __func__);
			return -ENXIO;
		}
	} else {
		dev_err(dev, "%s: cannot find node\n", __func__);
		return -ENXIO;
	}

	return stm_of_run_seq(state, seqnode);
}

/* stm device callbacks */

int stm_of_device_init(struct stm_device_state *state)
{
	return stm_of_run_device_seq(state, "init-seq");
}
int stm_of_device_exit(struct stm_device_state *state)
{
	return stm_of_run_device_seq(state, "exit-seq");
}

void stm_of_device_power(struct stm_device_state *state,
		enum stm_device_power_state power_state)
{
	if (power_state == stm_device_power_on)
		stm_of_run_device_seq(state, "power-on-seq");
	else
		stm_of_run_device_seq(state, "power-off-seq");

}

static int stm_of_parse_dev_sysconfs(struct device *dev,
			struct device_node *np,	struct device_node *sysconfs,
			struct stm_device_config *dc)
{
	const __be32 *list;
	struct property *pp;
	struct stm_device_sysconf *sysconf;
	phandle phandle;
	struct device_node *sysconf_groups;
	int i = 0, k, sysconf_cells, group_cells, nr_sysconfs = 0, nr_groups;
	const __be32 *ip;
	sysconf_cells = STM_OF_STM_DEV_SYCONF_CELLS;

	for_each_property_of_node(sysconfs, pp) {
		if (pp->length == (sysconf_cells * sizeof(u32)))
			nr_sysconfs++;
	}

	dc->sysconfs = devm_kzalloc(dev, (nr_sysconfs) * sizeof(*sysconf),
			GFP_KERNEL);

	for_each_property_of_node(sysconfs, pp) {
		const __be32 *group_list;
		struct property *sysconf_group;
		int sysconf_num;
		if (pp->length != (sysconf_cells * sizeof(u32)))
			continue;

		list = pp ? pp->value : NULL;
		sysconf = &dc->sysconfs[i++];

		/* sysconf group */
		phandle = be32_to_cpup(list++);
		sysconf_num = be32_to_cpup(list++);
		sysconf->regnum = sysconf_num;
		sysconf->lsb = be32_to_cpup(list++);
		sysconf->msb = be32_to_cpup(list++);
		sysconf->name = pp->name;

		sysconf_groups = of_find_node_by_phandle(phandle);

		ip = of_get_property(sysconf_groups,
					"#sysconf-group-cells", NULL);
		group_cells = be32_to_cpup(ip);
		if (group_cells < 3)
			continue;

		sysconf_group = of_find_property(sysconf_groups,
					"sysconf-groups", NULL);
		nr_groups = sysconf_group->length/(sizeof(u32) * group_cells);
		group_list = sysconf_group->value;
		for (k = 0; k < nr_groups; k++) {
			int start = be32_to_cpup(group_list++);
			int end = be32_to_cpup(group_list++);
			int group_num = be32_to_cpup(group_list++);
			if (sysconf_num >= start && sysconf_num <= end) {
				sysconf->regtype = group_num;
				sysconf->regnum  = sysconf_num - start;
				break;
			}
		}
	}
	return nr_sysconfs;
}
/**
 *	stm_of_parse_dev_config_direct - parse stm device config from a
 *				device config node.
 *	@np:	Node points to a device config node.
 *
 *	returns a pointer to newly allocated stm_deivice_config.
 *	User is responsible to freeing the pointer.
 *
 */
struct stm_device_config *stm_of_get_dev_config_from_node(struct device *dev,
					struct device_node *dc)
{
	struct stm_device_config *dev_config;
	struct device_node *sysconfs;
	if (!dc)
		return NULL;

	dev_config = devm_kzalloc(dev, sizeof(*dev_config), GFP_KERNEL);

	dev_config->pad_config = stm_of_get_pad_config_from_node(dev, dc, 0);

	sysconfs = of_get_child_by_name(dc, "sysconfs");
	if (sysconfs) {
		dev_config->sysconfs_num = stm_of_parse_dev_sysconfs(dev, dc,
						sysconfs, dev_config);
		of_node_put(sysconfs);
	}

	if (of_parse_phandle(dc, "device-seqs", 0)) {
		dev_config->init = stm_of_device_init;
		dev_config->exit = stm_of_device_exit;
		dev_config->power = stm_of_device_power;
	} else
		dev_dbg(dev, "%s : NO dev-seqs\n", __func__);
	return dev_config;
}
EXPORT_SYMBOL(stm_of_get_dev_config_from_node);
/**
 *	stm_of_get_dev_config - parse stm device config from a device pointer.
 *
 *	returns a pointer to newly allocated stm_device_config.
 *	User is responsible to freeing the pointer.
 *
 */
struct stm_device_config *stm_of_get_dev_config(struct device *dev)
{
	struct device_node *np = dev->of_node;
	return stm_of_get_dev_config_from_node(dev,
			of_parse_phandle(np, "device-config", 0));
}
EXPORT_SYMBOL(stm_of_get_dev_config);

static void stm_of_dev_conf_sysconf_fixup(struct device *dev,
		struct device_node *np,	struct stm_device_state *dev_state)
{
	struct property *pp;
	const __be32 *list;
	struct device_node *sysconfs;
	uint32_t val;
	sysconfs = of_get_child_by_name(np, "sysconfs");

	if (!sysconfs)
		return;

	for_each_property_of_node(sysconfs, pp) {
		if (pp->length != sizeof(u32))
			continue;
		list = pp->value;
		val = be32_to_cpup(list++);
		stm_device_sysconf_write(dev_state, pp->name, val);
	}
	of_node_put(sysconfs);
}
/**
 *	stm_of_dev_config_fixup - fixup a stm device config from a
 *				device config fixup node.
 *	@np:		Node points to a device config fixup node.
 *	@dev_state	previous stm_device_state object
 */
void stm_of_dev_config_fixup(struct device *dev, struct device_node *fixup_np,
				struct stm_device_state *dev_state)
{
	struct stm_pad_state *pad_state;
	pad_state = stm_device_get_pad_state(dev_state);
	stm_of_pad_config_fixup(dev, fixup_np, pad_state);
	stm_of_dev_conf_sysconf_fixup(dev, fixup_np, dev_state);
	return;
}
EXPORT_SYMBOL(stm_of_dev_config_fixup);
#endif
