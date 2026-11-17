/*
 * SPDX-License-Identifier: MIT
 * The text and the shapes of the dashboard page, apart from the drawing so
 * they can be tested on the host.
 */

#ifndef ZEPHCORE_UI_DASH_FMT_H
#define ZEPHCORE_UI_DASH_FMT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 999, 1.2k, 12k, 345k, 1M: at most 4 characters. */
static inline void ui_dash_count(char *out, size_t cap, uint32_t n)
{
	if (n < 1000) {
		snprintf(out, cap, "%u", (unsigned)n);
	} else if (n < 10000) {
		snprintf(out, cap, "%u.%uk", (unsigned)(n / 1000), (unsigned)((n % 1000) / 100));
	} else if (n < 1000000) {
		snprintf(out, cap, "%uk", (unsigned)(n / 1000));
	} else {
		snprintf(out, cap, "%uM", (unsigned)(n / 1000000));
	}
}

/* 45s, 4m, 3h, 2d. */
static inline void ui_dash_age(char *out, size_t cap, uint32_t s)
{
	if (s < 60) {
		snprintf(out, cap, "%us", (unsigned)s);
	} else if (s < 3600) {
		snprintf(out, cap, "%um", (unsigned)(s / 60));
	} else if (s < 86400) {
		snprintf(out, cap, "%uh", (unsigned)(s / 3600));
	} else {
		snprintf(out, cap, "%ud", (unsigned)(s / 86400));
	}
}

/* Bars lit, 0 to 4, of a signal that arrived at this SNR (a quarter of a dB each). */
static inline int ui_dash_bars(int snr_x4)
{
	return snr_x4 >= 32 ? 4 : snr_x4 >= 12 ? 3 : snr_x4 >= -12 ? 2 : snr_x4 >= -40 ? 1 : 0;
}

/* Width of the filled part of a gauge `width` wide: at least one pixel for any use. */
static inline int ui_dash_fill(uint32_t used, uint32_t total, int width)
{
	if (total == 0 || width <= 0) {
		return 0;
	}
	if (used >= total) {
		return width;
	}
	int w = (int)(((uint64_t)used * (uint64_t)width) / total);

	return (w == 0 && used > 0) ? 1 : w;
}

/* Percentage of the time spent on air, one decimal under 10 %: "0.4%", "12%". */
static inline void ui_dash_duty(char *out, size_t cap, uint32_t air_s, uint32_t uptime_s)
{
	if (uptime_s == 0) {
		snprintf(out, cap, "-");
		return;
	}
	uint32_t tenths = (uint32_t)(((uint64_t)air_s * 1000u) / uptime_s);

	if (tenths >= 100) {
		snprintf(out, cap, "%u%%", (unsigned)(tenths / 10));
	} else {
		snprintf(out, cap, "%u.%u%%", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
	}
}

/* 1d02h, 3h05m, 12m. */
static inline void ui_dash_uptime(char *out, size_t cap, uint32_t s)
{
	const uint32_t m = s / 60, h = m / 60, d = h / 24;

	if (d > 0) {
		snprintf(out, cap, "%ud%02uh", (unsigned)d, (unsigned)(h % 24));
	} else if (h > 0) {
		snprintf(out, cap, "%uh%02um", (unsigned)h, (unsigned)(m % 60));
	} else {
		snprintf(out, cap, "%um", (unsigned)m);
	}
}

#endif /* ZEPHCORE_UI_DASH_FMT_H */
