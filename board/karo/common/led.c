// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <console.h>
#include <errno.h>
#include <fdt_support.h>
#include <fsl_esdhc_imx.h>
#include <fsl_wdog.h>
#include <fuse.h>
#include <i2c.h>
#include <led.h>
#include <malloc.h>
#include <miiphy.h>
#include <mipi_dsi.h>
#include <mmc.h>
#include <netdev.h>
#include <spl.h>
#include <thermal.h>
#include <usb.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/armv8/mmu.h>
#include <asm/mach-imx/dma.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/video.h>
#include <asm/setup.h>
#include <asm/bootm.h>
#include <dm/uclass.h>
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
	LED_STATE_DISABLED,
};

static int led_state = LED_STATE_DISABLED;
static bool tx8m_temp_check_enabled = true;
static struct udevice *leddev;
static struct udevice *thermaldev;

#define TEMPERATURE_HOT		80
#define TEMPERATURE_MIN		-40

static inline int calc_blink_rate(void)
{
	int cpu_temp;
	static int last_temp = INT_MAX;
	static int avg_count;

	if (!tx8m_temp_check_enabled)
		return CONFIG_SYS_HZ;

	if (!thermaldev || thermal_get_temp(thermaldev, &cpu_temp))
		return CONFIG_SYS_HZ / 2;

	if (last_temp == INT_MAX) {
		last_temp = cpu_temp;
	} else if (cpu_temp != last_temp) {
		static int cpu_temps[4];

		if (thermal_get_temp(thermaldev, &cpu_temps[avg_count]))
			return CONFIG_SYS_HZ / 2;
		if (++avg_count >= ARRAY_SIZE(cpu_temps)) {
			int bad = -1;
			int i;

			for (i = 0; i < avg_count; i++) {
				if (cpu_temp != cpu_temps[i])
					bad = i;
			}
			if (bad < 0) {
				debug("CPU temperature changed from %d to %d\n",
				      last_temp, cpu_temp);
				last_temp = cpu_temp;
			} else {
				debug("Spurious CPU temperature reading %d -> %d -> %d\n",
				      cpu_temp, cpu_temps[bad],
				      cpu_temps[i - 1]);
			}
			avg_count = 0;
		}
	} else {
		avg_count = 0;
	}
	return CONFIG_SYS_HZ + CONFIG_SYS_HZ / 10 -
		(last_temp - TEMPERATURE_MIN) * CONFIG_SYS_HZ /
		(TEMPERATURE_HOT - TEMPERATURE_MIN);
}

void show_activity(int arg)
{
	static int blink_rate;
	static ulong last;
	int ret;

	if (led_state == LED_STATE_DISABLED)
		return;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		ret = led_set_state(leddev, LEDST_ON);
		if (ret == 0)
			led_state = LED_STATE_ON;
		else
			led_state = LED_STATE_DISABLED;
		blink_rate = calc_blink_rate();
	} else {
		if (get_timer(last) > blink_rate) {
			blink_rate = calc_blink_rate();
			last = get_timer(0);
			if (led_state == LED_STATE_ON)
				ret = led_set_state(leddev, LEDST_OFF);
			else
				ret = led_set_state(leddev, LEDST_ON);
			if (ret == 0)
				led_state = 1 - led_state;
			else
				led_state = LED_STATE_DISABLED;
		}
	}
}

void tx8m_led_init(void)
{
	int ret;
	int cpu_temp;

	ret = uclass_get_device_by_name(UCLASS_THERMAL, "cpu-thermal",
					&thermaldev);
	if (ret) {
		printf("Failed to find THERMAL device: %d\n", ret);
		return;
	}
	if (thermaldev && thermal_get_temp(thermaldev, &cpu_temp) == 0)
		printf("CPU temperature: %d C\n", cpu_temp);

	ret = led_get_by_label("Heartbeat", &leddev);
	if (ret) {
		if (ret != -ENODEV)
			printf("Failed to find LED device: %d\n", ret);
		return;
	}

	led_state = LED_STATE_INIT;
}
