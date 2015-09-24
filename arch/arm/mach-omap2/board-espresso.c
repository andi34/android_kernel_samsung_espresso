/* arch/arm/mach-omap2/board-espresso.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/reboot.h>
#include <linux/sysfs.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/cpu.h>
#include <plat/remoteproc.h>
#include <plat/usb.h>

#ifdef CONFIG_OMAP_HSI_DEVICE
#include <plat/omap_hsi.h>
#endif

#include <mach/dmm.h>
#include <mach/omap4-common.h>
#include <mach/id.h>
#ifdef CONFIG_ION_OMAP
#include <mach/omap4_ion.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-espresso.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_muxtbl.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define OMAP_SW_BOOT_CFG_ADDR	0x4A326FF8
#define REBOOT_FLAG_NORMAL		(1 << 0)
#define REBOOT_FLAG_RECOVERY	(1 << 1)
#define REBOOT_FLAG_POWER_OFF	(1 << 4)
#define REBOOT_FLAG_DOWNLOAD	(1 << 5)

#define ESPRESSO_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define ESPRESSO_RAMCONSOLE_SIZE	SZ_2M

#if defined(CONFIG_ANDROID_RAM_CONSOLE)
static struct resource ramconsole_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
		.start	= ESPRESSO_RAMCONSOLE_START,
		.end	= ESPRESSO_RAMCONSOLE_START
			+ ESPRESSO_RAMCONSOLE_SIZE - 1,
	 },
};

static struct platform_device ramconsole_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ramconsole_resources),
	.resource	= ramconsole_resources,
};
#endif /* CONFIG_ANDROID_RAM_CONSOLE */

static struct platform_device bcm4330_bluetooth_device = {
	.name		= "bcm4330_bluetooth",
	.id		= -1,
};

static struct platform_device *espresso_dbg_devices[] __initdata = {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	&ramconsole_device,
#endif
};

static struct platform_device *espresso_devices[] __initdata = {
	&bcm4330_bluetooth_device,
};

static void __init espresso_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);

	omap4_espresso_display_early_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode		= MUSB_OTG,
#else
	.mode		= MUSB_PERIPHERAL,
#endif
	.power		= 500,
};

#define CARRIER_WIFI_ONLY	"wifi-only"

static unsigned int board_type = SEC_MACHINE_ESPRESSO;

static int __init espresso_set_board_type(char *str)
{
	if (!strncmp(str, CARRIER_WIFI_ONLY, strlen(CARRIER_WIFI_ONLY)))
		board_type = SEC_MACHINE_ESPRESSO_WIFI;

	return 0;
}
__setup("androidboot.carrier=", espresso_set_board_type);

static unsigned int sec_vendor = SEC_MACHINE_ESPRESSO_WIFI;

static int __init espresso_set_vendor_type(char *str)
{
	unsigned int vendor;

	if (kstrtouint(str, 0, &vendor))
		return 0;

	if (vendor == 0)
		sec_vendor = SEC_MACHINE_ESPRESSO_USA_BBY;

	return 0;
}
__setup("sec_vendor=", espresso_set_vendor_type);

static void __init omap4_espresso_update_board_type(void)
{
	if (system_rev < 7)
		return;

	if (board_type == SEC_MACHINE_ESPRESSO_WIFI &&
	    sec_vendor == SEC_MACHINE_ESPRESSO_USA_BBY)
		board_type = SEC_MACHINE_ESPRESSO_USA_BBY;
}

unsigned int __init omap4_espresso_get_board_type(void)
{
	return board_type;
}

static int espresso_reboot_call(struct notifier_block *this,
				unsigned long code, void *cmd)
{
	u32 flag = REBOOT_FLAG_NORMAL;
	char *blcmd = "RESET";

	if (code == SYS_POWER_OFF) {
		flag = REBOOT_FLAG_POWER_OFF;
		blcmd = "POFF";
	} else if (code == SYS_RESTART) {
		if (cmd) {
			if (!strcmp(cmd, "recovery"))
				flag = REBOOT_FLAG_RECOVERY;
			else if (!strcmp(cmd, "download"))
				flag = REBOOT_FLAG_DOWNLOAD;
		}
	}

	omap_writel(flag, OMAP_SW_BOOT_CFG_ADDR);
	omap_writel(*(u32 *) blcmd, OMAP_SW_BOOT_CFG_ADDR - 0x04);

	return NOTIFY_DONE;
}

static struct notifier_block espresso_reboot_notifier = {
	.notifier_call = espresso_reboot_call,
};

static void __init sec_common_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Failed to create class (sec)!\n");
}

static void __init espresso_init(void)
{
	omap4_espresso_update_board_type();

	omap4_espresso_emif_init();
	sec_muxtbl_init(SEC_MACHINE_ESPRESSO, system_rev);

	register_reboot_notifier(&espresso_reboot_notifier);

	/* initialize sec class */
	sec_common_init();

	/* initialize each drivers */
	omap4_espresso_serial_init();
	omap4_espresso_charger_init();
	omap4_espresso_pmic_init();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	platform_add_devices(espresso_devices, ARRAY_SIZE(espresso_devices));
	omap_dmm_init();
	omap4_espresso_sdio_init();
	usb_musb_init(&musb_board_data);
	omap4_espresso_connector_init();
	omap4_espresso_display_init();
	omap4_espresso_input_init();
	omap4_espresso_wifi_init();
	omap4_espresso_sensors_init();
	omap4_espresso_jack_init();
	omap4_espresso_none_modem_init();

#ifdef CONFIG_OMAP_HSI_DEVICE
	/* Allow HSI omap_device to be registered later */
	omap_hsi_allow_registration();
#endif

	platform_add_devices(espresso_dbg_devices,
			     ARRAY_SIZE(espresso_dbg_devices));
}

static void __init espresso_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();
}

static void omap4_espresso_init_carveout_sizes(
		struct omap_ion_platform_data *ion)
{
	ion->tiler1d_size = (SZ_1M * 14);
	/* WFD is not supported in espresso So the size is zero */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 65);
	ion->nonsecure_tiler2d_size = (SZ_1M * 8);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init espresso_reserve(void)
{
#ifdef CONFIG_ION_OMAP
	omap_init_ram_size();
	omap4_espresso_memory_display_init();
	omap4_espresso_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();
#endif
	/* do the static reservations first */
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	memblock_remove(ESPRESSO_RAMCONSOLE_START,
			ESPRESSO_RAMCONSOLE_SIZE);
#endif
	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);

	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM,
				    PHYS_ADDR_DUCATI_SIZE +
				    OMAP4_ION_HEAP_SECURE_INPUT_SIZE +
				    OMAP4_ION_HEAP_SECURE_OUTPUT_WFDHDCP_SIZE);
	omap_reserve();
}

MACHINE_START(OMAP4_SAMSUNG, "Espresso")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params	= 0x80000100,
	.reserve	= espresso_reserve,
	.map_io		= espresso_map_io,
	.init_early	= espresso_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= espresso_init,
	.timer		= &omap_timer,
MACHINE_END
