/*
 * Copyright (C) 2012 STMicroelectronics
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interfaces (where required) the co-processors on ST platforms based
 * on multiprocessor architecture, for embedded products like Set-top-Box
 * DVD, etc...
 *
 */

#include <linux/device.h>
#include <linux/bpa2.h>
#include <linux/libelf.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/stm/soc.h>
#include <linux/stm/platform.h>

#include "coprocessor.h"

static struct class *coproc_class;

/* Stub character device interface */
static int coproc_dev_init(void)
{
	return 0;
}

static void copro_dev_exit(void)
{
}

static int coproc_dev_add(struct coproc *cop)
{
	cop->devt = 0;
	return 0;
}
static void coproc_dev_remove(struct coproc *cop) { }

static struct bpa2_part *coproc_get_bpa2_area(struct coproc *cop)
{
	char coproc_bpa2_name[COPROC_BPA2_NAME_LEN];

	scnprintf(coproc_bpa2_name, sizeof(coproc_bpa2_name),
		  "%s_%s%d", COPROC_BPA2_NAME,
		  cop->name, cop->id);

	return bpa2_find_part(coproc_bpa2_name);
}

static int coproc_load_elf(const struct firmware *fw, struct coproc *cop)
{
	int ret;
	struct ELF32_info *elfinfo = NULL;
	unsigned long boot_address;
	unsigned long n_pages;
	struct ELF32_LoadParams load_params = ELF_LOADPARAMS_INIT;

	elfinfo = ELF32_initFromMem((uint8_t *)fw->data, fw->size, 0);
	if (elfinfo == NULL) {
		coproc_dbg(cop, "Unable to parse ELF file\n");
		ret = -ENOMEM;
		goto err_elf;
	}

	if (elfinfo->header->e_type != ET_EXEC) {
		coproc_dbg(cop, "ELF file is not an executable\n");
		ret = -EINVAL;
		goto err_machine;
	}

	if (elfinfo->header->e_machine != cop->fns->machine) {
		coproc_dbg(cop, "Unexpected machine flag %d\n",
				elfinfo->header->e_machine);
		ret = -EINVAL;
		goto err_machine;
	}

	cop->bpa2_partition = coproc_get_bpa2_area(cop);
	if (!cop->bpa2_partition) {
		coproc_dbg(cop, "Unable to find BPA2 partition\n");
		ret = -ENOMEM;
		goto err_bpa2_get;
	}

	bpa2_memory(cop->bpa2_partition, &cop->ram_phys, &cop->ram_size);
	n_pages = cop->ram_size / PAGE_SIZE;
	cop->bpa2_alloc = bpa2_alloc_pages(cop->bpa2_partition, n_pages, 1,
							GFP_KERNEL);
	if (!cop->bpa2_alloc) {
		coproc_dbg(cop, "Unable to allocate memory from BPA2\n");
		ret = -ENOMEM;
		goto err_bpa2_alloc;
	}

	if (cop->fns->check_elf) {
		ret = cop->fns->check_elf(cop, elfinfo);
		if (ret)
			goto err_check_elf;
	}

	load_params.numAllowedRanges = 1;
	load_params.allowedRanges = kmalloc(sizeof(struct ELF32_MemRange),
					   GFP_KERNEL);
	if (!load_params.allowedRanges)
		goto err_load;
	load_params.allowedRanges[0].base = (Elf32_Addr)cop->ram_phys;
	load_params.allowedRanges[0].top = (Elf32_Addr)cop->ram_phys +
								cop->ram_size;
	ret = ELF32_physLoad(elfinfo, &load_params,
			     (Elf32_Addr *)&boot_address);
	ELF_LOADPARAMS_FREE(&load_params);
	if (ret)
		goto err_load;

	coproc_dbg(cop, "Received firmware size %d bytes\n", fw->size);
	coproc_dbg(cop, "cop->ram_size    = 0x%lx\n", cop->ram_size);
	coproc_dbg(cop, "cop->ram_phys  = 0x%lx\n", cop->ram_phys);
	coproc_dbg(cop, "boot address	= 0x%lx\n", boot_address);
	coproc_dbg(cop, "Run the Firmware code\n");

	ELF32_free(elfinfo);
	cop->fns->cpu_grant(cop, boot_address);
	return 0;

err_load:
err_check_elf:
	bpa2_free_pages(cop->bpa2_partition, cop->bpa2_alloc);
err_bpa2_alloc:
err_bpa2_get:
err_machine:
	ELF32_free(elfinfo);
err_elf:
	return ret;
}

static int coproc_open(struct coproc *cop)
{
	char firm_loaded[COPROC_FIRMWARE_NAME_LEN];
	const struct firmware *fw = NULL;
	int result;

	/*
	 * Build the firmware file name.
	 * We use the standard name: "st_firmware_<SoC>_<cop_name>.elf"
	 * to specify the video/audio device number
	 */
	result = scnprintf(firm_loaded, sizeof(firm_loaded),
			"%s_%s_%s%d.elf", COPROC_FIRMWARE_NAME,
			stm_soc(), cop->name, cop->id);

	coproc_dbg(cop, "Requesting the file %s\n", firm_loaded);
	result = request_firmware(&fw, firm_loaded, cop->dev);
	if (result) {
		coproc_err(cop, "Error on Firmware Download\n");
		goto err_request_fw;
	}

	result = coproc_load_elf(fw, cop);
	if (result)
		goto err_load;

	release_firmware(fw);
	return 0;

err_load:
	release_firmware(fw);
err_request_fw:
	return result;
}

static void coproc_close(struct coproc *cop)
{
	bpa2_free_pages(cop->bpa2_partition, cop->bpa2_alloc);
}

/* Start: ST-Coprocessor Device Attribute on SysFs*/
static ssize_t coproc_show_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct coproc *cop = dev_get_drvdata(dev);
	static const char *state_names[3] = {
		[coproc_state_idle] = "idle",
		[coproc_state_running] = "run",
		[coproc_state_paused] = "pause"
	};
	int i;
	int len = 0;

	for (i = 0; i < ARRAY_SIZE(state_names); i++)
		if (cop->state == i)
			len += sprintf(buf+len, "[%s] ", state_names[i]);
		else
			len += sprintf(buf+len, "%s ", state_names[i]);

	len += sprintf(len+buf, "\n");
	return len;
}

static ssize_t coproc_store_state(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct coproc *cop = dev_get_drvdata(dev);
	char request[16];
	int len;
	int res;

	/* Can't use strlcpy because it does a strlen first! */
	request[sizeof(request)-1] = '\0';
	strncpy(request, buf, sizeof(request)-1);
	len = strlen(request);
	if (len && request[len-1] ==  '\n')
		request[len - 1] = '\0';

	if (!strcmp("run", request)) {
		switch (cop->state) {
		case coproc_state_idle:
			res = coproc_open(cop);
			break;
		case coproc_state_paused:
			res = cop->fns->mode(cop, 1);
			break;
		default:
			res = -EINVAL;
			break;
		}
		if (!res)
			cop->state = coproc_state_running;
	} else
	if (!strcmp("pause", request)) {
		switch (cop->state) {
		case coproc_state_running:
			res = cop->fns->mode(cop, 0);
			break;
		default:
			res = -EINVAL;
			break;
		}
		if (!res)
			cop->state = coproc_state_paused;
	} else
	if (!strcmp("idle", request)) {
		switch (cop->state) {
		case coproc_state_running:
		case coproc_state_paused:
			res = cop->fns->mode(cop, 0);
			break;
		default:
			res = -EINVAL;
			break;
		}
		if (!res) {
			coproc_close(cop);
			cop->state = coproc_state_idle;
		}
	}

	return count;
}

static DEVICE_ATTR(state, S_IRWXUGO, coproc_show_state,
		coproc_store_state);

static ssize_t coproc_show_mem_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct coproc *cop = platform_get_drvdata(pdev);
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", cop->ram_size);
}

static DEVICE_ATTR(mem_size, S_IRUGO, coproc_show_mem_size, NULL);

static ssize_t coproc_show_mem_base(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct coproc *cop = platform_get_drvdata(pdev);
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", cop->ram_phys);
}

static DEVICE_ATTR(mem_base, S_IRUGO, coproc_show_mem_base, NULL);
/* End: ST-Coprocessor Device Attribute SysFs*/

int coproc_device_add(struct coproc *cop)
{
	int ret;

	ret = coproc_dev_add(cop);
	if (ret)
		goto err_dev_add;

	cop->dev = device_create(coproc_class, cop->parent, cop->devt, cop,
				 "%s.%d", cop->name, cop->id);
	if (IS_ERR(cop->dev)) {
		ret = PTR_ERR(cop->dev);
		goto err_device;
	}

	/* Now complete with the platform dependent init stage */
	ret = device_create_file(cop->dev, &dev_attr_state);
	if (ret)
		goto err_attr_state;
	ret = device_create_file(cop->dev, &dev_attr_mem_size);
	if (ret)
		goto err_attr_size;
	ret = device_create_file(cop->dev, &dev_attr_mem_base);
	if (ret)
		goto err_attr_base;

	coproc_info(cop, "coprocessor initialized\n");
	return 0;

err_attr_base:
	device_remove_file(cop->dev, &dev_attr_mem_size);
err_attr_size:
	device_remove_file(cop->dev, &dev_attr_state);
err_attr_state:
	device_unregister(cop->dev);
err_device:
	coproc_dev_remove(cop);
err_dev_add:
	return ret;
}
EXPORT_SYMBOL(coproc_device_add);

int coproc_device_remove(struct coproc *cop)
{
	device_remove_file(cop->dev, &dev_attr_state);
	device_remove_file(cop->dev, &dev_attr_mem_size);
	device_remove_file(cop->dev, &dev_attr_mem_base);
	device_unregister(cop->dev);
	return 0;
}
EXPORT_SYMBOL(coproc_device_remove);

static int __init coproc_init(void)
{
	int retval;

	printk(KERN_INFO "STMicroelectronics - Coprocessors Init\n");

	coproc_class = class_create(THIS_MODULE, "stm-coprocessor");
	if (IS_ERR(coproc_class)) {
		retval = PTR_ERR(coproc_class);
		printk(KERN_ERR "stm-coprocessor: can't register stm-coprocessor class\n");
		goto err_class;
	}

	retval = coproc_dev_init();
	if (retval)
		goto err_dev;

	return 0;

err_dev:
	class_destroy(coproc_class);
err_class:
	return retval;
}


static void __exit coproc_exit(void)
{
	copro_dev_exit();
	class_destroy(coproc_class);
	return;
}

MODULE_DESCRIPTION("Co-processor manager for multi-core devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.3");
MODULE_LICENSE("GPL");

subsys_initcall(coproc_init);
module_exit(coproc_exit);
