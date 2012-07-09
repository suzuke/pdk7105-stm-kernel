/*
 * STMicroelectronics FDMA dmaengine driver firmware functions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This code borrows heavily from drivers/stm/fdma.c!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/firmware.h>

#include <linux/stm/platform.h>

#include "stm_fdma.h"


static int stm_fdma_fw_check_header(struct stm_fdma_device *fdev,
		struct ELF32_info *elfinfo)
{
	int i;

	/* Check the firmware ELF header */
	if (elfinfo->header->e_type != ET_EXEC) {
		dev_err(fdev->dev, "Firmware invalid ELF type\n");
		return -EINVAL;
	}

	if (elfinfo->header->e_machine != EM_SLIM) {
		dev_err(fdev->dev, "Firmware invalid ELF machine\n");
		return -EINVAL;
	}

	if (elfinfo->header->e_flags != EF_SLIM_FDMA) {
		dev_err(fdev->dev, "Firmware invalid ELF flags\n");
		return -EINVAL;
	}

	/* We expect the firmware to contain only 2 loadable segments */
	if (elfinfo->header->e_phnum != STM_FDMA_FW_SEGMENTS) {
		dev_err(fdev->dev, "Firmware contains more than 2 segments\n");
		return -EINVAL;
	}

	for (i = 0; i < elfinfo->header->e_phnum; ++i) {
		if (elfinfo->progbase[i].p_type != PT_LOAD) {
			dev_err(fdev->dev, "Firmware segment %d not loadable\n",
				i);
			return -EINVAL;
		}
	}

	return 0;
}

static int stm_fdma_fw_check_segment(signed long offset,
			   unsigned long size, struct stm_plat_fdma_ram *ram)
{
	return (offset >= ram->offset) &&
		((offset + size) <= (ram->offset + ram->size));
}

static int stm_fdma_fw_copy_segment(struct stm_fdma_device *fdev,
		struct ELF32_info *elfinfo, int i)
{
	Elf32_Phdr *phdr = &elfinfo->progbase[i];
	void *data = elfinfo->base;
	signed long offset = phdr->p_paddr - fdev->io_res->start;
	unsigned long size = phdr->p_memsz;

	/* Check DMEM and IMEM segments are valid */
	if (!(stm_fdma_fw_check_segment(offset, size, &fdev->hw->dmem) ||
		stm_fdma_fw_check_segment(offset, size, &fdev->hw->imem))) {
		dev_err(fdev->dev, "Firmware segment check failed\n");
		return -EINVAL;
	}

	/* Copy the segment to the FDMA */
	memcpy_toio(fdev->io_base + offset, data + phdr->p_offset, size);

	return 0;
}

int stm_fdma_fw_load(struct stm_fdma_device *fdev, struct ELF32_info *elfinfo)
{
	int result;
	int i;

	BUG_ON(!fdev);
	BUG_ON(!elfinfo);

	/* Check the ELF file header */
	result = stm_fdma_fw_check_header(fdev, elfinfo);
	if (result) {
		dev_err(fdev->dev, "Firmware header check failed\n");
		return result;
	}

	/* Copy the firmware segments to the FDMA */
	for (i = 0; i < elfinfo->header->e_phnum; i++) {
		result = stm_fdma_fw_copy_segment(fdev, elfinfo, i);
		if (result) {
			dev_err(fdev->dev, "Failed to copy segment %d\n", i);
			return result;
		}
	}

	/* Initialise the FDMA */
	result = stm_fdma_hw_initialise(fdev);
	if (result) {
		dev_err(fdev->dev, "Failed to initialise FDMA\n");
		return result;
	}

	return 0;
}

static int stm_fdma_fw_request(struct stm_fdma_device *fdev)
{
	const struct firmware *fw = NULL;
	struct ELF32_info *elfinfo = NULL;
	int result = 0;
	int fw_major, fw_minor;
	int hw_major, hw_minor;

	BUG_ON(!fdev);

	/* Generate FDMA firmware file name */
	result = snprintf(fdev->fw_name, sizeof(fdev->fw_name),
			"fdma_%s_%d.elf", stm_soc(),
			(fdev->pdev->id == -1) ? 0 : fdev->pdev->id);
	BUG_ON(result >= sizeof(fdev->fw_name));

	dev_notice(fdev->dev, "Requesting firmware: %s\n", fdev->fw_name);

	/* Request the FDMA firmware */
	result = request_firmware(&fw, fdev->fw_name, fdev->dev);
	if (result || !fw) {
		dev_err(fdev->dev, "Failed request firmware: not present?\n");
		result = -ENODEV;
		goto error_no_fw;
	}

	/* Initialise firmware as an in-memory ELF file */
	elfinfo = (struct ELF32_info *)ELF32_initFromMem((uint8_t *) fw->data,
							fw->size, 0);
	if (elfinfo == NULL) {
		dev_err(fdev->dev, "Failed to initialise in-memory ELF file\n");
		result = -ENOMEM;
		goto error_elf_init;
	}

	/* Attempt to load the ELF file */
	result = stm_fdma_fw_load(fdev, elfinfo);
	if (result) {
		dev_err(fdev->dev, "Failed to load firmware\n");
		goto error_elf_load;
	}

	/* Retrieve the hardware and firmware versions */
	stm_fdma_hw_get_revisions(fdev, &hw_major, &hw_minor, &fw_major,
			&fw_minor);
	dev_notice(fdev->dev, "SLIM hw %d.%d, FDMA fw %d.%d\n",
			hw_major, hw_minor, fw_major, fw_minor);

	/* Indicate firmware loaded and save pointer to ELF for future reload */
	fdev->fw_state = STM_FDMA_FW_STATE_LOADED;
	fdev->fw_elfinfo = elfinfo;

	/* Wake up the wait queue */
	wake_up(&fdev->fw_load_q);

	/* Release the firmware */
	release_firmware(fw);

	return 0;

error_elf_load:
	ELF32_free(elfinfo);
error_elf_init:
	release_firmware(fw);
error_no_fw:
	fdev->fw_state = STM_FDMA_FW_STATE_ERROR;
	return result;
}

int stm_fdma_fw_check(struct stm_fdma_device *fdev)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdev->lock, irqflags);

	switch (fdev->fw_state) {
	case STM_FDMA_FW_STATE_INIT:
		/* Firmware is not loaded, so start the process */
		fdev->fw_state = STM_FDMA_FW_STATE_LOADING;
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return stm_fdma_fw_request(fdev);

	case STM_FDMA_FW_STATE_LOADING:
		/* Firmware is loading, so wait until state changes */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		wait_event_interruptible(fdev->fw_load_q,
				fdev->fw_state != STM_FDMA_FW_STATE_LOADING);
		return fdev->fw_state == STM_FDMA_FW_STATE_LOADED ? 0 : -ENODEV;

	case STM_FDMA_FW_STATE_LOADED:
		/* Firmware has loaded */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return 0;

	case STM_FDMA_FW_STATE_ERROR:
		/* Firmware error */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return -ENODEV;

	default:
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		dev_err(fdev->dev, "Invalid firmware state: %d\n",
			fdev->fw_state);
		return -ENODEV;
	}

	return 0;
}
