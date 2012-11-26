/*
 *  stmfp_infra.c
 */


#include <linux/stm/stig125.h>
#include <linux/stm/stig125-periphs.h>
#include <linux/clk.h>
#include <linux/stmfp.h>
#include <linux/stm/pad.h>

struct fastpath_clk_rate {
	char *clk_n;
	unsigned long rate;
};

int setup_fp_clocks(void)
{
	static int fp_clk_rate_init_done;
	int ret, i;

	static struct fastpath_clk_rate fastpath_clks[] = {
		{
			.clk_n = "CLK_S_E_VCO",
			.rate = 600000000,
		}, {
			.clk_n = "CLK_S_E_FP",
			.rate = 250000000,
		}, {
			.clk_n = "CLK_S_E_D3_XP70",
			.rate = 324000000,
		}, {
			.clk_n = "CLK_S_E_IFE",
			.rate = 216000000,
		}, {
			.clk_n = "CLK_S_E_IFE_WB",
			.rate = 324000000,
		}, {
			.clk_n = "CLK_S_D_VCO",
			.rate = 600000000,
		}, {
			.clk_n = "CLK_S_D_ISIS_ETH",
			.rate = 250000000,
		}
	};

	if (fp_clk_rate_init_done)
		return 0;

	printk(KERN_INFO "setup_clocks() for fastpath\n");
	for (i = 0; i < ARRAY_SIZE(fastpath_clks); ++i) {
		struct clk *clk;
		clk = clk_get(NULL, fastpath_clks[i].clk_n);
		if (IS_ERR(clk)) {
			ret = -EINVAL;
			goto on_error;
		}
		clk_prepare_enable(clk);
		ret = clk_set_rate(clk, fastpath_clks[i].rate);
		if (ret)
			goto on_error;
		clk_put(clk);
	}

	fp_clk_rate_init_done = 1;
	return ret;

on_error:
	pr_err("Error %d: setup FP clock %s to clock rate %lu\n", ret,
	       fastpath_clks[i].clk_n, fastpath_clks[i].rate);
	return ret;

}  /* end setup_fp_clocks() */


int stmfp_claim_resources(void *ptr)
{
	struct plat_fpif_data *plat = ptr;
	void *pad;
	int ret;

	pad = stm_pad_claim(plat->pad_config, "fpif");
	if (pad == NULL)
		return -EIO;
	plat->pad_state = pad;
	ret =  setup_fp_clocks();
	if (ret)
		stm_pad_release(plat->pad_state);

	return ret;
}

void stmfp_release_resources(void *ptr)
{
	struct plat_fpif_data *plat = ptr;
	/* release the pad (used for pio and retimer settings */
	stm_pad_release(plat->pad_state);
	return;
}
