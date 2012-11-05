
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/byteorder/generic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_mtd.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/pio-control.h>
#include <linux/stm/platform.h>
/**
*	stm_of_get_partitions_node - get partitions node from stm-nand
*				type devices.
*	@dev		device pointer to use for devm allocations.
*	@np		device node of the driver.
*	@bank_nr	which bank number to use to get partitions.
*
*	Returns a node pointer if found, with refcount incremented, use
*	of_node_put() on it when done.
*
*/
struct device_node *stm_of_get_partitions_node(struct device_node *np,
		int bank_nr)
{
	char name[10];
	struct device_node *banks, *bank, *parts = NULL;
	banks = of_parse_phandle(np, "st,nand-banks", 0);

	if (banks) {
		sprintf(name, "bank%d", bank_nr);
		bank = of_get_child_by_name(banks, name);
		if (bank) {
			parts = of_get_child_by_name(bank, "partitions");
			of_node_put(bank);
		}
	}

	return parts;
}
EXPORT_SYMBOL(stm_of_get_partitions_node);

/**
 *	stm_of_get_nand_banks - Get nand banks info from a given device node.
 *
 *	@dev			device pointer to use for devm allocations.
 *	@np			device node of the driver.
 *	@banksp			double pointer to banks which is allocated
 *				and filled with bank data.
 *
 *	Returns a count of banks found in the given device node.
 *
 */
int stm_of_get_nand_banks(struct device *dev, struct device_node *np,
		struct stm_nand_bank_data **banksp)
{
	int i;
	struct device_node *bank_np = NULL;
	int nr_banks = 0;
	struct stm_nand_bank_data *banks;
	struct device_node *banks_np;
	if (!np)
		return -ENODEV;

	banks_np = of_parse_phandle(np, "st,nand-banks", 0);
	while ((bank_np = of_get_next_child(banks_np, bank_np)))
		nr_banks++;
	*banksp = devm_kzalloc(dev, sizeof(*banks) * nr_banks, GFP_KERNEL);
	banks = *banksp;
	bank_np = NULL;
	for (i = 0; i < nr_banks; i++) {
		struct device_node *timing;
		bank_np = of_get_next_child(banks_np, bank_np);
		of_property_read_u32(bank_np, "st,nand-csn", &banks[i].csn);

		if (of_get_nand_bus_width(bank_np) == 16)
			banks[i].options |= NAND_BUSWIDTH_16;
		if (of_get_nand_on_flash_bbt(bank_np))
			banks[i].bbt_options |= NAND_BBT_USE_FLASH;
		if (of_property_read_bool(bank_np, "nand-no-autoincr"))
			banks[i].options |= NAND_NO_AUTOINCR;

		banks[i].nr_partitions = 0;
		banks[i].partitions = NULL;
		timing = of_parse_phandle(bank_np, "st,nand-timing-data", 0);
		banks[i].timing_data = devm_kzalloc(dev,
			sizeof(struct stm_nand_timing_data), GFP_KERNEL);
		of_property_read_u32(timing, "sig-setup",
					&banks[i].timing_data->sig_setup);
		of_property_read_u32(timing, "sig-hold",
					&banks[i].timing_data->sig_hold);
		of_property_read_u32(timing, "CE-deassert",
					&banks[i].timing_data->CE_deassert);
		of_property_read_u32(timing, "WE-to-RBn",
					&banks[i].timing_data->WE_to_RBn);
		of_property_read_u32(timing, "wr-on",
					&banks[i].timing_data->wr_on);
		of_property_read_u32(timing, "wr-off",
					&banks[i].timing_data->wr_off);
		of_property_read_u32(timing, "rd-on",
					&banks[i].timing_data->rd_on);
		of_property_read_u32(timing, "rd-off",
					&banks[i].timing_data->rd_off);
		of_property_read_u32(timing, "chip-delay",
					&banks[i].timing_data->chip_delay);

	}
	return nr_banks;
}
EXPORT_SYMBOL(stm_of_get_nand_banks);
