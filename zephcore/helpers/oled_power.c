/*
 * SPDX-License-Identifier: MIT
 * ZephCore - OLED display power management
 *
 * Sends I2C display-off command to SH1106/SSD1306 OLED to save power.
 * The Wio Tracker L1's SH1106 has no hardware power gate, so this is
 * the only way to prevent it from drawing ~20mA in standby.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zephcore_oled_power, CONFIG_ZEPHCORE_BOARD_LOG_LEVEL);

#include "oled_power.h"

/* The bus and the address come from the devicetree node of the panel itself, so
 * a board without one compiles this out and a panel at another address or on
 * another bus still gets the command. Never a hardcoded address: writing to an
 * unidentified device is what this avoids. Zephyr spells the per-compatible
 * switch DT_COMPAT_HAS_OKAY_<compat>, not DT_HAS_<COMPAT>_ENABLED. */
#if IS_ENABLED(CONFIG_I2C) && \
	(DT_COMPAT_HAS_OKAY_sinowealth_sh1106 || DT_COMPAT_HAS_OKAY_solomon_ssd1306)

#include <zephyr/drivers/i2c.h>

#if DT_COMPAT_HAS_OKAY_sinowealth_sh1106
#define OLED_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(sinowealth_sh1106)
#else
#define OLED_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(solomon_ssd1306)
#endif

static const struct i2c_dt_spec oled_bus = I2C_DT_SPEC_GET(OLED_NODE);

void oled_sleep(void)
{
	if (!device_is_ready(oled_bus.bus)) {
		return;
	}

	uint8_t cmd[] = {0x00, 0xAE};  /* Display OFF */
	int ret = i2c_write_dt(&oled_bus, cmd, sizeof(cmd));
	if (ret == 0) {
		LOG_INF("OLED display put to sleep (power saving)");
	} else {
		LOG_DBG("OLED sleep failed (ret=%d)", ret);
	}
}

#else

void oled_sleep(void) { }

#endif
