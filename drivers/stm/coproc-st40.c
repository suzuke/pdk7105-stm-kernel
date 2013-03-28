#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>

#include "coprocessor.h"

#define DRIVER_NAME "stm-coproc-st40"

struct coproc_st40 {
	struct coproc coproc;
	struct stm_device_state *coproc_device_state;
	int (*cpu_boot)(struct stm_device_state *dev_state,
					unsigned long boot_addr,
					unsigned long phy_addr);
	int (*stbus_req_filter)(struct stm_device_state *dev_state,
				int mode);
};

static struct coproc_st40 *coproc_to_coproc_st40(struct coproc *cop)
{
	return container_of(cop, struct coproc_st40, coproc);
}

static int coproc_st40_cpu_grant(struct coproc *cop, unsigned long boot_addr)
{
	struct coproc_st40 *cop_st40 = coproc_to_coproc_st40(cop);
	coproc_dbg(cop, "cpu_grant: starts from 0x%lx...\n", boot_addr);

	if (cop_st40->cpu_boot)
		return cop_st40->cpu_boot(cop_st40->coproc_device_state,
					boot_addr, cop->ram_phys);

	return 0;
}

static int coproc_st40_mode(struct coproc *cop, int mode)
{
	struct coproc_st40 *cop_st40 = coproc_to_coproc_st40(cop);
	if (cop_st40->stbus_req_filter)
		cop_st40->stbus_req_filter(cop_st40->coproc_device_state, mode);

	return 0;
}

/* Address translation range 64 MB */
#define BART_AT_RANGE		0x4000000
/* 6 Bits [31:26] */
#define BART_ADDR_MASK		0xFC000000

static int coproc_st40_check_elf(struct coproc *cop, struct ELF32_info *elfinfo)
{
	struct coproc_st40 *cop_st40 = coproc_to_coproc_st40(cop);
	unsigned long boot_address = elfinfo->header->e_entry;
	unsigned long boot_address_phys = 0 ;
	unsigned long long bart_lock_status;

	bart_lock_status = stm_device_sysconf_read(
					cop_st40->coproc_device_state,
					"BART_LOCK_ENABLE_STATUS");

	if (bart_lock_status == 1) { /* BART  Locked  */
		unsigned long bart_bootaddr = 0;
		Elf32_Phdr *phdr = elfinfo->progbase;
		int lmi0_not_lmi1, lmi_offset_baseaddr, i;

		/* find the matching fw phy load address */
		for (i = 0; i < elfinfo->header->e_phnum; i++) {
			if (phdr[i].p_type == PT_LOAD) {
				unsigned int vaddr = phdr[i].p_vaddr;
				unsigned int size = phdr[i].p_filesz;
				if (boot_address >= vaddr &&
					boot_address <= (vaddr + size)) {
					boot_address_phys = phdr[i].p_paddr;
					break;
				}
			}
		}

		lmi0_not_lmi1 = stm_device_sysconf_read(
					cop_st40->coproc_device_state,
					"BART_LMI0_NOT_LMI1_SEL_STATUS");
		lmi_offset_baseaddr = stm_device_sysconf_read(
					cop_st40->coproc_device_state,
					"BART_LMI_OFFSET_BASEADDR_STATUS");

		bart_bootaddr = ((lmi0_not_lmi1 ? 0x1 : 0x2) << 30) |
					(lmi_offset_baseaddr << 26);

		coproc_info(cop,
			    "BART Frozen to translate 64MB from 0x%lx\n",
			    bart_bootaddr);

		boot_address_phys = boot_address_phys & BART_ADDR_MASK;
		if (boot_address_phys >= bart_bootaddr &&
			boot_address_phys < (bart_bootaddr + BART_AT_RANGE))
			return 0;
		else {
			coproc_info(cop, "ST40 Cannot boot from 0x%lx"
					" loaded at 0x%lx\n",
					boot_address, boot_address_phys);

			return -EINVAL;
		}

	} /* Either not locked or bart sysconf bits not passed by SOC layer */

	return 0;
}

struct coproc_fns coproc_st40_fns = {
	.machine = EM_SH,
	.check_elf = coproc_st40_check_elf,
	.cpu_grant = coproc_st40_cpu_grant,
	.mode = coproc_st40_mode,
};

#ifdef CONFIG_OF
void *coproc_st40_get_pdata(struct platform_device *pdev)
{
	struct plat_stm_st40_coproc_data *data;
	struct device_node *np = pdev->dev.of_node;
	data = dev_get_platdata(&pdev->dev);

	if (!data)
		data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);

	of_property_read_string(np, "proc-name", &data->name);

	data->device_config = stm_of_get_dev_config(&pdev->dev);
	data->id = of_alias_get_id(np, "coproc-st40");

	return data;
}
#else
void *coproc_st40_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif
static int coproc_st40_driver_probe(struct platform_device *pdev)
{
	struct plat_stm_st40_coproc_data *cop_data;
	struct coproc_st40 *cop_st40;
	int result;

	if (pdev->dev.of_node)
		cop_data = coproc_st40_get_pdata(pdev);
	else
		cop_data = dev_get_platdata(&pdev->dev);

	cop_st40 = devm_kzalloc(&pdev->dev, sizeof(*cop_st40), GFP_KERNEL);
	if (!cop_st40) {
		result = -ENOMEM;
		goto err;
	}

	cop_st40->coproc.name = cop_data->name;
	cop_st40->coproc.id = cop_data->id;
	cop_st40->coproc.parent = &pdev->dev;
	cop_st40->coproc.fns = &coproc_st40_fns;
	cop_st40->cpu_boot = cop_data->cpu_boot;
	cop_st40->stbus_req_filter = cop_data->stbus_req_filter;

	cop_st40->coproc_device_state = devm_stm_device_init(&pdev->dev,
						cop_data->device_config);
	if (!cop_st40->coproc_device_state) {
		result = -EBUSY;
		goto err;
	}

	result = coproc_device_add(&cop_st40->coproc);
	if (result)
		goto err;

	platform_set_drvdata(pdev, cop_st40);
	return result;

err:
	return result;
}

static int coproc_st40_driver_remove(struct platform_device *pdev)
{
	struct coproc_st40 *cop_st40 = platform_get_drvdata(pdev);

	coproc_device_remove(&cop_st40->coproc);
	return 0;
}

static struct of_device_id coproc_st40_match[] = {
	{
		.compatible = "st,coproc-st40",
	},
	{},
};
static struct platform_driver coproc_st40_driver = {
	.driver.name = DRIVER_NAME,
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(coproc_st40_match),
	.probe = coproc_st40_driver_probe,
	.remove = coproc_st40_driver_remove,
};

static int __init coproc_st40_init(void)
{
	return platform_driver_register(&coproc_st40_driver);
}


static void __exit coproc_st40_exit(void)
{
	platform_driver_unregister(&coproc_st40_driver);
}

MODULE_DESCRIPTION("Co-processor manager for multi-core devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.3");
MODULE_LICENSE("GPL");

module_init(coproc_st40_init);
module_exit(coproc_st40_exit);
