/*
 * SPDX-License-Identifier: MIT
 *
 * RP2040 bring-up diagnostic, see rp2040_boot_diag.h. Uses busy-waits only, so
 * it works before the scheduler runs and does not depend on the USB stack.
 */

#include "rp2040_boot_diag.h"

#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

static const struct gpio_dt_spec diag_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static void diag_pulse(uint32_t on_us, uint32_t off_us)
{
	gpio_pin_set_dt(&diag_led, 1);
	k_busy_wait(on_us);
	gpio_pin_set_dt(&diag_led, 0);
	k_busy_wait(off_us);
}

void rp2040_diag_blink(int n)
{
	if (!gpio_is_ready_dt(&diag_led)) {
		return;
	}
	gpio_pin_configure_dt(&diag_led, GPIO_OUTPUT_INACTIVE);
	k_busy_wait(1000000);
	for (int i = 0; i < n; i++) {
		diag_pulse(200000, 300000);
	}
}

void rp2040_diag_result(int rc)
{
	if (rc == 0) {
		rp2040_diag_blink(4);
		return;
	}

	if (!gpio_is_ready_dt(&diag_led)) {
		return;
	}
	gpio_pin_configure_dt(&diag_led, GPIO_OUTPUT_INACTIVE);
	k_busy_wait(1000000);
	for (int i = 0; i < 10; i++) {
		diag_pulse(60000, 60000);
	}
	rp2040_diag_blink(abs(rc));
}

static int diag_stage1(void)
{
	rp2040_diag_blink(1);
	return 0;
}

static int diag_stage2(void)
{
	rp2040_diag_blink(2);
	return 0;
}

SYS_INIT(diag_stage1, PRE_KERNEL_2, 0);
SYS_INIT(diag_stage2, APPLICATION, 99);
