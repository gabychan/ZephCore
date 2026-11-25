/*
 * SPDX-License-Identifier: MIT
 * ZephCore LoRa timing helpers shared by the sx126x, lr11xx and lr20xx drivers.
 *
 * Pure functions of the modem parameters, so every driver bounds its RX-busy
 * gate with the same figures, and the host unit tests can check them.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_LORA_ZC_LORA_TIMING_H_
#define ZEPHYR_INCLUDE_DRIVERS_LORA_ZC_LORA_TIMING_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Preamble grace for the RX-busy gate: (preamble + 8) symbols, rounded up to a
 * whole ms. Enough for a real packet's header to land after PREAMBLE_DETECTED;
 * a foreign sync word releases the gate once it expires. bw_hz == 0 or an SF
 * outside 5..12 means "not configured yet" and yields 1 s. */
static inline uint32_t zc_lora_preamble_grace_ms(uint8_t sf, uint32_t bw_hz,
						  uint16_t preamble)
{
	if (bw_hz == 0 || sf < 5 || sf > 12) {
		return 1000;
	}
	uint64_t us = ((uint64_t)(preamble + 8U) << sf) * 1000000ULL / bw_hz;

	return (uint32_t)((us + 999U) / 1000U);
}

/* Upper bound on a packet's payload phase: airtime of a 255-byte explicit-header
 * packet at CR 4/8 with LDRO on (the larger symbol count), +25% +100 ms. Bounds
 * the header latch, which continuous RX has no timer to clear. Deliberately
 * generous: releasing early would let TX start over a packet still arriving.
 * Unconfigured (see above) yields 30 s. */
static inline uint32_t zc_lora_max_payload_ms(uint8_t sf, uint32_t bw_hz)
{
	if (bw_hz == 0 || sf < 5 || sf > 12) {
		return 30000;
	}

	/* n = 8 + ceil((8*PL - 4*SF + 28 + 16) / (4*(SF - 2*DE))) * 8,
	 * PL = 255, DE = 1: the divisor is 4*(SF-2), never zero at SF >= 5. */
	uint32_t numer = 8U * 255U + 28U + 16U;
	uint32_t denom = 4U * (uint32_t)(sf - 2U);

	if (numer > 4U * (uint32_t)sf) {
		numer -= 4U * (uint32_t)sf;
	}
	uint32_t n_sym = 8U + ((numer + denom - 1U) / denom) * 8U;

	uint64_t us = ((uint64_t)n_sym << sf) * 1000000ULL / bw_hz;
	uint32_t ms = (uint32_t)((us + 999U) / 1000U);

	return ms + (ms / 4U) + 100U;
}

/* ms -> a 24-bit chip timer count at step_hz, computed in 64 bits and
 * saturated at 0xFFFFFF. Masking instead of saturating wraps a long timeout
 * into a short one (SF12/BW7.8 exceeds 24 bits at 15.625 us steps). */
static inline uint32_t zc_lora_ms_to_steps24(uint32_t ms, uint32_t step_hz)
{
	uint64_t steps = ((uint64_t)ms * step_hz) / 1000U;

	return steps > 0x00FFFFFFU ? 0x00FFFFFFU : (uint32_t)steps;
}

/* RX duty-cycle window arithmetic (SX1261/2 DS Rev 2.2 §13.1.7, AN1200.36):
 *   catch:    sleep + wake <= (P - 2D - 1) * Tsym
 *   complete: 2R + S >= (P + 14) * Tsym
 *   floor:    R >= (D + 1) * Tsym
 * The catch budget is derated by margin_pct: the RC sleep timer drifts and
 * the wake time "may vary", so an overshoot drops phase-edge packets. The
 * caller owns the preamble in force (a Meshtastic peer forces 16 symbols) and
 * the wake estimate, and the two rejections, so its log strings stay put. */
enum zc_lora_rx_duty_reason {
	ZC_LORA_RX_DUTY_OK = 0,
	ZC_LORA_RX_DUTY_PREAMBLE_SHORT, /* bw_hz == 0, or P <= 2D + 1 */
	ZC_LORA_RX_DUTY_WAKE_TOO_LONG,  /* budget <= wake + 2000 us */
};

struct zc_lora_rx_duty {
	uint32_t rx_us;
	uint32_t sleep_us;
	uint32_t trans_us;
	uint32_t deaf_us;  /* the derated catch budget, for the report */
	enum zc_lora_rx_duty_reason reason;
};

static inline struct zc_lora_rx_duty
zc_lora_rx_duty_window(uint8_t sf, uint32_t bw_hz, uint16_t preamble,
		       uint16_t detect_syms, uint32_t margin_pct,
		       uint32_t trans_us)
{
	struct zc_lora_rx_duty w = {0U, 0U, trans_us, 0U,
				    ZC_LORA_RX_DUTY_PREAMBLE_SHORT};

	if (bw_hz == 0U || preamble <= 2 * detect_syms + 1) {
		return w;
	}

	const uint32_t sym_us = (uint32_t)(((uint64_t)(1U << sf) * 1000000ULL) / bw_hz);
	const uint32_t deaf_budget_us =
		(uint32_t)(preamble - 2 * detect_syms - 1) * sym_us;

	w.deaf_us = deaf_budget_us -
		(uint32_t)(((uint64_t)deaf_budget_us * margin_pct) / 100U);
	if (w.deaf_us <= trans_us + 2000U) {
		w.reason = ZC_LORA_RX_DUTY_WAKE_TOO_LONG;
		return w;
	}

	w.sleep_us = w.deaf_us - trans_us;

	const uint32_t complete_us = (uint32_t)(preamble + 14) * sym_us;
	const uint32_t floor_us = (uint32_t)(detect_syms + 1) * sym_us;
	const uint32_t need_us = complete_us > w.sleep_us
					 ? (complete_us - w.sleep_us + 1) / 2 : 0U;

	w.rx_us = need_us > floor_us ? need_us : floor_us;
	w.reason = ZC_LORA_RX_DUTY_OK;
	return w;
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_LORA_ZC_LORA_TIMING_H_ */
