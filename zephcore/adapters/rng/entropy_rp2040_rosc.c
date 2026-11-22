/*
 * SPDX-License-Identifier: MIT
 *
 * ZephCore — RP2040 entropy source from the ring oscillator (ROSC).
 *
 * The RP2040 has no hardware RNG and this Zephyr tree has an entropy driver
 * only for the RP2350. ROSC RANDOMBIT returns one jittery bit per read; it is
 * not a certified TRNG. Each output bit is the XOR of ROSC_SAMPLES_PER_BIT
 * reads spaced by a short busy-wait, and the driver is registered with
 * ENTROPY_NEEDS_PRNG so Zephyr runs a DRBG on top of it (see
 * Kconfig ZEPHCORE_ENTROPY_RP2040_ROSC). ZephyrRNG then conditions identity
 * seeds with CPU jitter as well.
 *
 * Stateless: concurrent callers just interleave reads, so no lock is held with
 * interrupts off for the ~64 us per byte this takes.
 */

#define DT_DRV_COMPAT zephcore_rp2040_rosc_entropy

#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>

#define ROSC_BASE		0x40060000UL
#define ROSC_CTRL		(ROSC_BASE + 0x00)
#define ROSC_STATUS		(ROSC_BASE + 0x18)
#define ROSC_RANDOMBIT		(ROSC_BASE + 0x1c)

#define ROSC_CTRL_ENABLE_MASK	(0xfffUL << 12)
#define ROSC_CTRL_ENABLE_VAL	(0xfabUL << 12)
#define ROSC_STATUS_STABLE	BIT(31)

#define ROSC_SAMPLES_PER_BIT	8
#define ROSC_SAMPLE_GAP_US	1
#define ROSC_STABLE_TIMEOUT_US	1000

static int rosc_ensure_running(void)
{
	uint32_t ctrl = sys_read32(ROSC_CTRL);

	if ((ctrl & ROSC_CTRL_ENABLE_MASK) != ROSC_CTRL_ENABLE_VAL) {
		sys_write32((ctrl & ~ROSC_CTRL_ENABLE_MASK) | ROSC_CTRL_ENABLE_VAL, ROSC_CTRL);
	}

	for (int waited = 0; waited < ROSC_STABLE_TIMEOUT_US; waited++) {
		if (sys_read32(ROSC_STATUS) & ROSC_STATUS_STABLE) {
			return 0;
		}
		k_busy_wait(1);
	}

	return -EIO;
}

static int entropy_rp2040_rosc_get_entropy(const struct device *dev, uint8_t *buf, uint16_t len)
{
	ARG_UNUSED(dev);

	int rc = rosc_ensure_running();

	if (rc != 0) {
		return rc;
	}

	for (uint16_t i = 0; i < len; i++) {
		uint8_t byte = 0;

		for (int bit = 0; bit < 8; bit++) {
			uint32_t b = 0;

			for (int s = 0; s < ROSC_SAMPLES_PER_BIT; s++) {
				b ^= sys_read32(ROSC_RANDOMBIT) & 1U;
				k_busy_wait(ROSC_SAMPLE_GAP_US);
			}
			byte = (uint8_t)((byte << 1) | b);
		}
		buf[i] = byte;
	}

	return 0;
}

static DEVICE_API(entropy, entropy_rp2040_rosc_api) = {
	.get_entropy = entropy_rp2040_rosc_get_entropy,
};

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
		      PRE_KERNEL_1, CONFIG_ENTROPY_INIT_PRIORITY,
		      &entropy_rp2040_rosc_api);
