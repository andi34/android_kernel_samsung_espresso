/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyreset.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/battery.h>
#include <linux/delay.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/touchscreen/synaptics.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-espresso10.h"
#include "control.h"

#define GPIO_EXT_WAKEUP	3
#define GPIO_VOL_UP		8
#define GPIO_VOL_DN		30

#define GPIO_TSP_VENDOR1	71
#define GPIO_TSP_VENDOR2	72
#define GPIO_TSP_VENDOR3	92

#define GPIO_TSP_INT        46
#define GPIO_TSP_LDO_ON     54
#define GPIO_TSP_I2C_SCL_1.8V        130
#define GPIO_TSP_I2C_SDA_1.8V        131

static struct gpio_event_direct_entry espresso_gpio_keypad_keys_map_high[] = {
	{
		.code	= KEY_POWER,
		.gpio	= GPIO_EXT_WAKEUP
	},
};

static struct gpio_event_input_info espresso_gpio_keypad_keys_info_high = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= espresso_gpio_keypad_keys_map_high,
	.keymap_size	= ARRAY_SIZE(espresso_gpio_keypad_keys_map_high),
	.flags			= GPIOEDF_ACTIVE_HIGH,
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_direct_entry espresso_gpio_keypad_keys_map_low[] = {
	{
		.code	= KEY_VOLUMEDOWN,
		.gpio	= GPIO_VOL_DN,
	},
	{
		.code	= KEY_VOLUMEUP,
		.gpio	= GPIO_VOL_UP,
	},
};

static struct gpio_event_input_info espresso_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= espresso_gpio_keypad_keys_map_low,
	.keymap_size	= ARRAY_SIZE(espresso_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *espresso_gpio_keypad_info[] = {
	&espresso_gpio_keypad_keys_info_high.info,
	&espresso_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data espresso_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= espresso_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(espresso_gpio_keypad_info)
};

static struct platform_device espresso_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &espresso_gpio_keypad_data,
	},
};

enum {
	NUM_TOUCH_nINT = 0,
	NUM_TOUCH_EN,
	NUM_TOUCH_SCL,
	NUM_TOUCH_SDA,
};

static struct gpio tsp_gpios[] = {
	[NUM_TOUCH_nINT] = {
		.flags	= GPIOF_IN,
		.label	= "TSP_INT",
		.gpio	= GPIO_TSP_INT,
	},
	[NUM_TOUCH_EN] = {
		.flags	= GPIOF_DIR_OUT,
		.label	= "TSP_LDO_ON",
		.gpio	= GPIO_TSP_LDO_ON,
	},
	[NUM_TOUCH_SCL] = {
		.label	= "TSP_I2C_SCL_1.8V",
		.gpio	= GPIO_TSP_I2C_SCL_1.8V,
	},
	[NUM_TOUCH_SDA] = {
		.label	= "TSP_I2C_SDA_1.8V",
		.gpio	= GPIO_TSP_I2C_SDA_1.8V,
	},
};

static void tsp_set_power(bool on)
{
	u32 r;

	if (on) {
		pr_debug("tsp: power on.\n");
		gpio_set_value(tsp_gpios[NUM_TOUCH_EN].gpio, 1);

		r = omap4_ctrl_pad_readl(
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r &= ~OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r &= ~OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r,
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);

		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
					tsp_gpios[NUM_TOUCH_nINT].gpio);
		msleep(300);
	} else {
		pr_debug("tsp: power off.\n");
		gpio_set_value(tsp_gpios[NUM_TOUCH_EN].gpio, 0);

		/* Below register settings need to prevent current leakage. */
		r = omap4_ctrl_pad_readl(
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r |= OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r |= OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r,
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);

		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
					tsp_gpios[NUM_TOUCH_nINT].gpio);
		msleep(50);
	}
	return;
}

static struct synaptics_fw_info espresso10_tsp_fw_info = {
	.release_date = "0906",
};

static struct sec_ts_platform_data espresso10_ts_pdata = {
	.fw_name	= "synaptics/p5100.fw",
	.fw_info	= &espresso10_tsp_fw_info,
	.ext_fw_name	= "/mnt/sdcard/p5100.img",
	.rx_channel_no	= 42,	/* Y channel line */
	.tx_channel_no	= 27,	/* X channel line */
	.x_pixel_size	= 1279,
	.y_pixel_size	= 799,
	.pivot		= false,
	.ta_state	= CABLE_NONE,
	.set_power	= tsp_set_power,
};

static struct i2c_board_info __initdata espresso_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("synaptics_ts", 0x20),
		.platform_data	= &espresso10_ts_pdata,
	},
};

static struct gpio ts_panel_gpios[] = {
	{
		.label	= "TSP_VENDOR1",
		.flags	= GPIOF_IN,
		.gpio	= GPIO_TSP_VENDOR1,
	},
	{
		.label	= "TSP_VENDOR2",
		.flags	= GPIOF_IN,
		.gpio	= GPIO_TSP_VENDOR2,
	},
	{
		.label	= "TSP_VENDOR3",
		.flags	= GPIOF_IN,
		.gpio	= GPIO_TSP_VENDOR3,
	},
};

static char *panel_name[8] = {"iljin", "o-film", "s-mac", };

static void __init espresso10_ts_panel_setup(void)
{
	int i, panel_id = 0;

	gpio_request_array(ts_panel_gpios, ARRAY_SIZE(ts_panel_gpios));

	for (i = 0; i < ARRAY_SIZE(ts_panel_gpios); i++)
		panel_id |= gpio_get_value(ts_panel_gpios[i].gpio) << i;

	espresso10_ts_pdata.panel_name = panel_name[panel_id];
}

static void __init espresso10_tsp_gpio_init(void)
{
	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	espresso_i2c3_boardinfo[0].irq =
				gpio_to_irq(tsp_gpios[NUM_TOUCH_nINT].gpio);

	espresso10_ts_pdata.gpio_en = tsp_gpios[NUM_TOUCH_EN].gpio;
	espresso10_ts_pdata.gpio_irq = tsp_gpios[NUM_TOUCH_nINT].gpio;
	espresso10_ts_pdata.gpio_scl = tsp_gpios[NUM_TOUCH_SCL].gpio;
	espresso10_ts_pdata.gpio_sda = tsp_gpios[NUM_TOUCH_SDA].gpio;
}

void omap4_espresso10_tsp_ta_detect(int cable_type)
{
	switch (cable_type) {
	case CABLE_TYPE_AC:
	espresso10_ts_pdata.ta_state = CABLE_TA;
	break;
	case CABLE_TYPE_USB:
	espresso10_ts_pdata.ta_state = CABLE_USB;
	break;
	case CABLE_TYPE_NONE:
	default:
	espresso10_ts_pdata.ta_state = CABLE_NONE;
	}

	/* Conditions for prevent kernel panic */
	if (espresso10_ts_pdata.set_ta_mode &&
				gpio_get_value(tsp_gpios[GPIO_TOUCH_EN].gpio))
		espresso10_ts_pdata.set_ta_mode(&espresso10_ts_pdata.ta_state);
	return;
}

void __init omap4_espresso10_input_init(void)
{
	u32 boardtype = omap4_espresso10_get_board_type();

	if (boardtype == SEC_MACHINE_ESPRESSO10_WIFI)
		espresso10_ts_pdata.model_name = "P5110";
	else if (boardtype == SEC_MACHINE_ESPRESSO10_USA_BBY)
		espresso10_ts_pdata.model_name = "P5113";
	else
		espresso10_ts_pdata.model_name = "P5100";

	if (system_rev >= 7)
		espresso10_ts_panel_setup();

	espresso10_tsp_gpio_init();

	i2c_register_board_info(3, espresso_i2c3_boardinfo,
				ARRAY_SIZE(espresso_i2c3_boardinfo));

	platform_device_register(&espresso_gpio_keypad_device);
}
