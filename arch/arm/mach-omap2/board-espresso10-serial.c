/* arch/arm/mach-omap2/board-espresso10-serial.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso-serial.c
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <plat/common.h>
#include <plat/omap_hwmod.h>
#include <plat/omap-serial.h>

#include "board-espresso10.h"
#include "mux.h"

#define GPIO_GPS_PWR_EN 173
#define GPIO_GPS_nRST   178

#define SDA_PIN_ADC_I2C 66
#define SCL_PIN_ADC_I2C 65

#define SDA_PIN_MHL	98
#define SCL_PIN_MHL	99

static struct i2c_board_info __initdata espresso10_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("ducati", 0x20),
		.irq		= OMAP44XX_IRQ_I2C2,
		.ext_master	= true,
	},
};

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request();
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", \
								bus_id);
	}
}

static struct omap_i2c_bus_board_data __initdata espresso_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata espresso_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata espresso_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata espresso_i2c_4_bus_pdata;

static void __init espresso10_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &espresso_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &espresso_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &espresso_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &espresso_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &espresso_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &espresso_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &espresso_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &espresso_i2c_4_bus_pdata);


	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
	omap_register_i2c_bus(1, 400, NULL, 0);
	omap_register_i2c_bus(2, 400, espresso10_i2c_board_info,
			      ARRAY_SIZE(espresso10_i2c_board_info));
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);
}

static struct i2c_gpio_platform_data espresso10_gpio_i2c6_pdata = {
	.sda_pin = SDA_PIN_ADC_I2C, 
	.scl_pin = SCL_PIN_ADC_I2C,
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device espresso10_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &espresso10_gpio_i2c6_pdata,
	}
};

static struct i2c_gpio_platform_data espresso10_gpio_i2c8_pdata = {
	.sda_pin      = SDA_PIN_MHL,
	.scl_pin      = SCL_PIN_MHL,
	.udelay         = 3,
	.timeout	= 0,
};

static struct platform_device espresso10_gpio_i2c8_device = {
	.name = "i2c-gpio",
	.id = 8,
	.dev = {
		.platform_data = &espresso10_gpio_i2c8_pdata,
	},
};

enum {
	NUM_GPS_PWR_EN = 0,
	NUM_GPS_nRST
};

static void espresso10_bcmgps_init(void)
{
	struct device *gps_dev;
	struct gpio gps_gpios[] = {
		[NUM_GPS_PWR_EN] = {
			.flags = GPIOF_OUT_INIT_LOW,
			.label = "GPS_PWR_EN",
			.gpio  = GPIO_GPS_PWR_EN,
		},
		[NUM_GPS_nRST] = {
			.flags = GPIOF_OUT_INIT_HIGH,
			.label = "GPS_nRST",
			.gpio  = GPIO_GPS_nRST,
		},
	};

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("(%s): failed to created device (gps)!\n", __func__);
		return;
	}

	gpio_request_array(gps_gpios, ARRAY_SIZE(gps_gpios));

	gpio_export(gps_gpios[NUM_GPS_PWR_EN].gpio, 1);
	gpio_export(gps_gpios[NUM_GPS_nRST].gpio, 1);

	gpio_export_link(gps_dev, gps_gpios[NUM_GPS_PWR_EN].label,
			 gps_gpios[NUM_GPS_PWR_EN].gpio);
	gpio_export_link(gps_dev, gps_gpios[NUM_GPS_nRST].label,
			 gps_gpios[NUM_GPS_nRST].gpio);
}

static struct omap_device_pad espresso10_uart1_pads[] __initdata = {
	{
		.name	= "mcspi1_cs3.uart1_rts",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name	= "mcspi1_cs2.uart1_cts",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
	},
	{
		.name   = "uart3_cts_rctx.uart1_tx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name   = "mcspi1_cs1.uart1_rx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
		.idle   = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
	},
};

static struct omap_device_pad espresso10_uart2_pads[] __initdata = {
	{
		.name	= "uart2_cts.uart2_cts",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rts.uart2_rts",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_tx.uart2_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rx.uart2_rx",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad espresso10_uart3_pads[] __initdata = {
	{
		.name   = "uart3_tx_irtx.uart3_tx_irtx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rx_irrx.uart3_rx_irrx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad espresso10_uart4_pads[] __initdata = {
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_uart_port_info espresso10_uart2_info __initdata = {
	.use_dma		= 0,
	.dma_rx_buf_size	= DEFAULT_RXDMA_BUFSIZE,
	.dma_rx_poll_rate	= DEFAULT_RXDMA_POLLRATE,
	.dma_rx_timeout		= DEFAULT_RXDMA_TIMEOUT,
	.auto_sus_timeout	= 0,
	.wake_peer		= bcm_bt_lpm_exit_lpm_locked,
	.rts_mux_driver_control	= 1,
};

static void __init omap_serial_none_pads_cfg_mux(void)
{
	struct omap_mux_partition *core = omap_mux_get("core");

	omap_mux_write(core,
		OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		OMAP4_CTRL_MODULE_PAD_UART3_RX_IRRX_OFFSET);

	omap_mux_write(core,
		OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		OMAP4_CTRL_MODULE_PAD_UART3_TX_IRTX_OFFSET);
}

static void __init espresso10_uart_init(void)
{
	omap_serial_init_port_pads(0, espresso10_uart1_pads,
				   ARRAY_SIZE(espresso10_uart1_pads), NULL);
	omap_serial_init_port_pads(1, espresso10_uart2_pads,
				   ARRAY_SIZE(espresso10_uart2_pads),
				   &espresso10_uart2_info);
	omap_serial_init_port_pads(2, espresso10_uart3_pads,
				   ARRAY_SIZE(espresso10_uart3_pads), NULL);
	omap_serial_init_port_pads(3, espresso10_uart4_pads,
				   ARRAY_SIZE(espresso10_uart4_pads), NULL);

	espresso10_bcmgps_init();
}

static struct platform_device *espresso10_serial_devices[] __initdata = {
	&espresso10_gpio_i2c6_device,
	&espresso10_gpio_i2c8_device,
};

void __init omap4_espresso10_serial_init(void)
{
	espresso10_i2c_init();
	espresso10_uart_init();

	platform_add_devices(espresso10_serial_devices,
			     ARRAY_SIZE(espresso10_serial_devices));
}

int __init omap4_espresso10_serial_late_init(void)
{
	unsigned int boardtype = omap4_espresso10_get_board_type();

	if (system_rev > 6 && boardtype == SEC_MACHINE_ESPRESSO10)
		omap_serial_none_pads_cfg_mux();

	return 0;
}

late_initcall(omap4_espresso10_serial_late_init);
