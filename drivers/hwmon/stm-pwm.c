/*
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Contains code copyright (C) Echostar Technologies Corporation
 * Author: Anthony Jackson <anthony.jackson@echostar.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <asm/io.h>

struct stm_pwm {
	struct resource *mem;
	void* base;
	struct device *hwmon_dev;
	struct stm_plat_pwm_data *platform_data;
};

/* PWM registers */
#define PWM_VAL(x)		(0x00 + (4 * (x)))
#define PWM_CTRL		0x50
#define PWM_CTRL_PWM_EN			(1<<9)
#define PWM_CTRL_PWM_CLK_VAL0_SHIFT	0
#define PWM_CTRL_PWM_CLK_VAL0_MASK	0x0f
#define PWM_CTRL_PWM_CLK_VAL4_SHIFT	11
#define PWM_CTRL_PWM_CLK_VAL4_MASK	0xf0
#define PWM_INT_EN		0x54

/* Prescale value (clock dividor):
 * 0: divide by 1
 * 0xff: divide by 256 */
#define PWM_CLK_VAL		0

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;
	struct stm_pwm *pwm = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", readl(pwm->base + PWM_VAL(channel)));
}

static ssize_t store_pwm(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int channel = to_sensor_dev_attr(attr)->index;
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	char* p;
	long val = simple_strtol(buf, &p, 10);

	if (p != buf) {
		val &= 0xff;
		writel(val, pwm->base + PWM_VAL(channel));
		return p-buf;
	}
	return -EINVAL;
}

static mode_t stm_pwm_is_visible(struct kobject *kobj, struct attribute *attr,
				int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	struct device_attribute *devattr;
	int channel;

	devattr = container_of(attr, struct device_attribute, attr);
	channel = to_sensor_dev_attr(devattr)->index;

	if (!pwm->platform_data->channel_enabled[channel])
		return 0;

	return attr->mode;
}

static SENSOR_DEVICE_ATTR(pwm0, S_IRUGO | S_IWUSR, show_pwm, store_pwm, 0);
static SENSOR_DEVICE_ATTR(pwm1, S_IRUGO | S_IWUSR, show_pwm, store_pwm, 1);
static SENSOR_DEVICE_ATTR(pwm2, S_IRUGO | S_IWUSR, show_pwm, store_pwm, 2);
static SENSOR_DEVICE_ATTR(pwm3, S_IRUGO | S_IWUSR, show_pwm, store_pwm, 3);

static struct attribute *stm_pwm_attributes[] = {
	&sensor_dev_attr_pwm0.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	NULL
};

static struct attribute_group stm_pwm_attr_group = {
	.is_visible = stm_pwm_is_visible,
	.attrs = stm_pwm_attributes,
};

static int
stm_pwm_init(struct platform_device  *pdev, struct stm_pwm *pwm)
{
	u32 reg = 0;
	int channel;

	/* disable PWM if currently running */
	reg = readl(pwm->base + PWM_CTRL);
	reg &= ~PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);

	/* disable all PWM related interrupts */
	reg = 0;
	writel(reg, pwm->base + PWM_INT_EN);

	/* Set global PWM state:
	 * disable capture... */
	reg = 0;

	/* set prescale value... */
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL0_MASK) << PWM_CTRL_PWM_CLK_VAL0_SHIFT;
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL4_MASK) << PWM_CTRL_PWM_CLK_VAL4_SHIFT;

	/* enable */
	reg |= PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);

	for (channel = 0; channel < STM_PLAT_PWM_NUM_CHANNELS; channel++) {
		if (pwm->platform_data->channel_enabled[channel]) {
			/* Initial value */
			writel(0, pwm->base + PWM_VAL(channel));
			if (!devm_stm_pad_claim(&pdev->dev,
				pwm->platform_data->channel_pad_config[channel],
				dev_name(&pdev->dev)))
				return -ENODEV;
		}
	}

	return sysfs_create_group(&pdev->dev.kobj, &stm_pwm_attr_group);
}

static int stm_pwm_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct stm_pwm *pwm;
	int err;

	pwm = kmalloc(sizeof(struct stm_pwm), GFP_KERNEL);
	if (pwm == NULL) {
		return -ENOMEM;
	}
	memset(pwm, 0, sizeof(*pwm));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!res) {
                err = -ENODEV;
		goto failed1;
	}

	pwm->mem = request_mem_region(res->start, res->end - res->start + 1, "stm-pwn");
	if (pwm->mem == NULL) {
		dev_err(&pdev->dev, "failed to claim memory region\n");
                err = -EBUSY;
		goto failed1;
	}

	pwm->base = ioremap(res->start, res->end - res->start + 1);
	if (pwm->base == NULL) {
		dev_err(&pdev->dev, "failed ioremap");
		err = -EINVAL;
		goto failed2;
	}

	pwm->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(pwm->hwmon_dev)) {
		err = PTR_ERR(pwm->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto failed3;
	}

	pwm->platform_data = pdev->dev.platform_data;

	platform_set_drvdata(pdev, pwm);
	dev_info(&pdev->dev, "registers at 0x%x, mapped to 0x%p\n",
		 res->start, pwm->base);

	return stm_pwm_init(pdev, pwm);

failed3:
	iounmap(pwm->base);
failed2:
	release_resource(pwm->mem);
failed1:
	kfree(pwm);
	return err;
}

static int stm_pwm_remove(struct platform_device *pdev)
{
	struct stm_pwm *pwm = platform_get_drvdata(pdev);

	if (pwm) {
		hwmon_device_unregister(pwm->hwmon_dev);
		sysfs_remove_group(&pdev->dev.kobj, &stm_pwm_attr_group);
		platform_set_drvdata(pdev, NULL);
		iounmap(pwm->base);
		release_resource(pwm->mem);
		kfree(pwm);
	}
	return 0;
}

static struct platform_driver stm_pwm_driver = {
	.driver = {
		.name		= "stm-pwm",
	},
	.probe		= stm_pwm_probe,
	.remove		= stm_pwm_remove,
};

static int __init stm_pwm_module_init(void)
{
	return platform_driver_register(&stm_pwm_driver);
}

static void __exit stm_pwm_module_exit(void)
{
	platform_driver_unregister(&stm_pwm_driver);
}

module_init(stm_pwm_module_init);
module_exit(stm_pwm_module_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics simple PWM driver");
MODULE_LICENSE("GPL");
