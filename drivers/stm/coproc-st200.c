#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/stm/platform.h>

#include "coprocessor.h"

#define DRIVER_NAME "stm-coproc-st200"

struct coproc_st231 {
	struct coproc coproc;
	struct stm_device_state *coproc_device_state;
	int (*reset_bypass)(enum stm_cpu_reset_bypass reset_bypass);
	int (*stbus_req_filter)(struct stm_device_state *state, int on);
	int boot_shift;
	int not_reset;
};

static struct coproc_st231 *coproc_to_coproc_st231(struct coproc *cop)
{
	return container_of(cop, struct coproc_st231, coproc);
}

static int coproc_st231_cpu_grant(struct coproc *cop, unsigned long boot_addr)
{
	struct coproc_st231 *cop_st231 = coproc_to_coproc_st231(cop);
	struct stm_device_state *dev_state = cop_st231->coproc_device_state;

	dev_dbg(cop->parent, "cpu_grant: starts from 0x%lx...\n", boot_addr);

	/* Only upper bits of boot address can be specified */
	if (boot_addr & ((1<<cop_st231->boot_shift)-1))
		return -EINVAL;

	/*
	 * Unset the CPU's "allow boot" - needed for STi7141 and older
	 * SoCs don't mind.
	 */
	if (cop_st231->stbus_req_filter)
		cop_st231->stbus_req_filter(dev_state, 0);

	/* Bypass CPU reset out signals */
	if (cop_st231->reset_bypass)
		cop_st231->reset_bypass(stm_bypass_st40_st231_handshake);

	/* Configure the boot address */
	stm_device_sysconf_write(dev_state, "BOOT_ADDR",
					boot_addr >> cop_st231->boot_shift);

	/* Assert and de-assert reset */
	if (cop_st231->not_reset) {
		stm_device_sysconf_write(dev_state, "RESET" , 0);
		stm_device_sysconf_write(dev_state, "RESET" , 1);
	} else {
		stm_device_sysconf_write(dev_state, "RESET" , 1);
		stm_device_sysconf_write(dev_state, "RESET" , 0);
	}

	/*
	 * Set the CPU's "allow boot" - this must be *after* the reset
	 * cycle on STi7141 - older SoCs don't mind.
	 */
	if (cop_st231->stbus_req_filter)
		cop_st231->stbus_req_filter(dev_state, 1);

	/* Remove bypass CPU reset out signals */
	if (cop_st231->reset_bypass)
		cop_st231->reset_bypass(stm_bypass_none);
	return 0;
}

static int coproc_st231_mode(struct coproc *cop, int mode)
{
	struct coproc_st231 *cop_st231 = coproc_to_coproc_st231(cop);

	if (cop_st231->stbus_req_filter)
		cop_st231->stbus_req_filter(cop_st231->coproc_device_state,
									mode);

	return 0;
}

static int coproc_st231_check_elf(struct coproc *cop,
				  struct ELF32_info *elfinfo)
{
	Elf32_Ehdr *hdr;
	Elf32_Shdr *sechdrs;
	unsigned long base_address = -1; /* 0xFFFFFFFF */
	int boot_index = 0, prev_index = 0;

	/* Convenience variables */
	hdr = elfinfo->header;
	sechdrs = elfinfo->secbase;

	/* Jump to section header */
	ELF32_getSectionByNameCheck(elfinfo, ".boot", &boot_index,
				SHF_ALLOC, SHT_PROGBITS);
	prev_index = ELF32_findBaseAddrCheck(hdr, sechdrs,
			 &base_address, SHF_ALLOC, SHT_PROGBITS);
	if (boot_index != 0) {
		sechdrs[boot_index].sh_addr =
			(sechdrs[prev_index].sh_addr +
			sechdrs[prev_index].sh_size +
			(1 << 8)) & ~((1 << 8) - 1);
		coproc_dbg(cop, "Rewriting entry point to %08lx\n",
			   (unsigned long)hdr->e_entry);
		hdr->e_entry = sechdrs[boot_index].sh_addr;
	}

	return 0;
}

struct coproc_fns coproc_st231_fns = {
	.machine = 0x64, /* EM_ST200 */
	.check_elf = coproc_st231_check_elf,
	.cpu_grant = coproc_st231_cpu_grant,
	.mode = coproc_st231_mode,
};
#ifdef CONFIG_OF
void *coproc_st231_get_pdata(struct platform_device *pdev)
{
	struct plat_stm_st231_coproc_data *data;
	struct device_node *np = pdev->dev.of_node;
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	of_property_read_string(np, "proc-name", &data->name);

	of_property_read_u32(np, "boot-shift", &data->boot_shift);
	if (of_property_read_bool(np, "not-reset"))
		data->not_reset = 1;
	data->device_config = stm_of_get_dev_config(&pdev->dev);
	data->id = of_alias_get_id(np, "coproc-st200");

	return data;
}
#else

void *coproc_st231_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif
static int coproc_st231_driver_probe(struct platform_device *pdev)
{
	struct plat_stm_st231_coproc_data *cop_data;
	struct coproc_st231 *cop_st231;
	int result;

	if (pdev->dev.of_node)
		cop_data = coproc_st231_get_pdata(pdev);
	else
		cop_data = dev_get_platdata(&pdev->dev);

	cop_st231 = devm_kzalloc(&pdev->dev, sizeof(*cop_st231), GFP_KERNEL);
	if (!cop_st231) {
		result = -ENOMEM;
		goto err;
	}

	cop_st231->coproc.name = cop_data->name;
	cop_st231->coproc.id = cop_data->id;
	cop_st231->reset_bypass = cop_data->reset_bypass;
	cop_st231->boot_shift = cop_data->boot_shift;
	cop_st231->not_reset = cop_data->not_reset;
	cop_st231->stbus_req_filter = cop_data->stbus_req_filter;
	cop_st231->coproc.parent = &pdev->dev;
	cop_st231->coproc.fns = &coproc_st231_fns;

	cop_st231->coproc_device_state = devm_stm_device_init(&pdev->dev,
						cop_data->device_config);
	if (!cop_st231->coproc_device_state) {
		result = -EBUSY;
		goto err;
	}

	result = coproc_device_add(&cop_st231->coproc);
	if (result)
		goto err;

	platform_set_drvdata(pdev, cop_st231);
	return result;

err:
	return result;
}

static int coproc_st231_driver_remove(struct platform_device *pdev)
{
	struct coproc_st231 *cop_st231 = platform_get_drvdata(pdev);

	coproc_device_remove(&cop_st231->coproc);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id coproc_st231_match[] = {
	{
		.compatible = "st,coproc-st200",
	},
	{},
};
MODULE_DEVICE_TABLE(of, coproc_st231_match);
#endif

static struct platform_driver coproc_st231_driver = {
	.driver.name = DRIVER_NAME,
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(coproc_st231_match),
	.probe = coproc_st231_driver_probe,
	.remove = coproc_st231_driver_remove,
};

static int __init coproc_st231_init(void)
{
	return platform_driver_register(&coproc_st231_driver);
}


static void __exit coproc_st231_exit(void)
{
	platform_driver_unregister(&coproc_st231_driver);
}

MODULE_DESCRIPTION("Co-processor manager for multi-core devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.3");
MODULE_LICENSE("GPL");

module_init(coproc_st231_init);
module_exit(coproc_st231_exit);
