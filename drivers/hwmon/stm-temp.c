/*
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>

struct stm_temp_sensor {
	struct platform_device *pdev;
	struct stm_device_state *device_state;
	struct thermal_zone_device *th_dev;
	struct plat_stm_temp_data *plat_data;
	unsigned long (*custom_get_data)(void *priv);
};

static int stm_thermal_get_temp(struct thermal_zone_device *th,
		unsigned long *temperature)
{
	struct stm_temp_sensor *sensor =
		(struct stm_temp_sensor *)th->devdata;
	unsigned long data;
	int overflow;

	pm_runtime_get_sync(&sensor->pdev->dev);

	overflow = stm_device_sysconf_read(sensor->device_state, "OVERFLOW");

	if (sensor->plat_data->custom_get_data)
		data = sensor->plat_data->custom_get_data(
				sensor->plat_data->custom_priv);
	else
		data = stm_device_sysconf_read(sensor->device_state, "DATA");

	overflow |= stm_device_sysconf_read(sensor->device_state, "OVERFLOW");

	*temperature = (data + sensor->plat_data->correction_factor) * 1000;

	pm_runtime_put(&sensor->pdev->dev);

	return overflow;
}

static struct thermal_zone_device_ops stm_thermal_ops = {
	.get_temp = stm_thermal_get_temp,
};
static void *stm_temp_get_pdata(struct platform_device *pdev)
{
	struct plat_stm_temp_data *data;
	struct device_node *np = pdev->dev.of_node;
	if (!np)
		return pdev->dev.platform_data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	data->device_config = stm_of_get_dev_config(&pdev->dev);
	if (of_property_read_bool(np, "st,calibrated"))
		data->calibrated = 1;
	of_property_read_u32(np, "st,calibration-value",
					&data->calibration_value);

	return data;
}
static int __devinit stm_temp_probe(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);
	struct plat_stm_temp_data *plat_data;

	plat_data = stm_temp_get_pdata(pdev);

	sensor = devm_kzalloc(&pdev->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(&pdev->dev, "Out of memory!\n");
		return -ENOMEM;
	}

	sensor->pdev = pdev;
	sensor->plat_data = plat_data;

	sensor->device_state = devm_stm_device_init(&pdev->dev,
		plat_data->device_config);

	if (!sensor->device_state)
		return  -EBUSY;

	if (plat_data->custom_set_dcorrect) {
		plat_data->custom_set_dcorrect(plat_data->custom_priv);
	} else {
		if (!plat_data->calibrated)
			plat_data->calibration_value = 16;

		stm_device_sysconf_write(sensor->device_state, "DCORRECT",
						plat_data->calibration_value);
	}

	platform_set_drvdata(pdev, sensor);

	sensor->th_dev = thermal_zone_device_register(
		(char *)dev_name(&pdev->dev), 0, (void *)sensor,
		&stm_thermal_ops, 0, 0, 10000, 10000);

	if (IS_ERR(sensor->th_dev))
		return -EBUSY;

	/* Initialize the pm_runtime fields */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

}

static int __devexit stm_temp_remove(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);
	stm_device_power(sensor->device_state, stm_device_power_off);
	thermal_zone_device_unregister(sensor->th_dev);
	return 0;
}

#ifdef CONFIG_PM
static int stm_temp_suspend(struct device *dev)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status != RPM_ACTIVE)
		return 0; /* sensor already suspended via runtime_suspend */
#endif
	stm_device_power(sensor->device_state, stm_device_power_off);
	return 0;
}

static int stm_temp_resume(struct device *dev)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);

#ifdef CONFIG_PM_RUNTIME
	if (dev->power.runtime_status == RPM_SUSPENDED)
		return 0; /* sensor wants resume via runtime_resume... */
#endif
	stm_device_power(sensor->device_state, stm_device_power_on);
	return 0;
}

static int stm_temp_restore(struct device *dev)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);
	stm_device_setup(sensor->device_state);

	return stm_temp_resume(dev);
}

static struct dev_pm_ops stm_temp_pm = {
	.suspend = stm_temp_suspend,  /* on standby/memstandby */
	.resume = stm_temp_resume,    /* resume from standby/memstandby */
	.freeze = stm_temp_suspend,
	.thaw = stm_temp_restore,
	.restore = stm_temp_restore,
	.runtime_suspend = stm_temp_suspend,
	.runtime_resume = stm_temp_resume,
};
#else
static struct dev_pm_ops stm_temp_pm;
#endif

#ifdef CONFIG_OF
static struct of_device_id stm_temp_match[] = {
	{
		.compatible = "st,temp",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_temp_match);
#endif

static struct platform_driver stm_temp_driver = {
	.driver = {
		.name	= "stm-temp",
		.of_match_table = of_match_ptr(stm_temp_match),
		.pm = &stm_temp_pm,
	},
	.probe		= stm_temp_probe,
	.remove		= stm_temp_remove,
};

static int __init stm_temp_init(void)
{
	return platform_driver_register(&stm_temp_driver);
}

static void __exit stm_temp_exit(void)
{
	platform_driver_unregister(&stm_temp_driver);
}

module_init(stm_temp_init);
module_exit(stm_temp_exit);

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics SOC internal temperature sensor driver");
MODULE_LICENSE("GPL");
