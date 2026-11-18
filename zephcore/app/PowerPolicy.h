/*
 * SPDX-License-Identifier: MIT
 *
 * The low-battery policy: the companion on every build that reads a battery,
 * with or without a UI, and a repeater or room server whose board wakes itself
 * when the battery recovers (an LPCOMP node in the devicetree): auto-shutdown
 * (prefs.auto_shutdown_mv, `get|set autoshutdown`) and the v-contact low-battery
 * alert (prefs.v_battery_alert_mv).
 * Driven by the housekeeping tick, main thread only.
 *
 * Both sample the battery every 30 s and need three low readings in a row, so
 * a TX sag cannot trigger them; both skip while the board reports external
 * power. A reading under PowerPolicy::NO_CELL_MV is "no battery" (a board
 * without a cell reads 0, or a floating divider), never a reason to act.
 */

#pragma once

#include <mesh/MeshCore.h>
#include <helpers/NodePrefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ZEPHYR__
#include <zephyr/devicetree.h>
/* A repeater or room server acts on auto-shutdown only where the board has an
 * LPCOMP node to wake it again and the option is on. */
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_lpcomp) && \
	defined(CONFIG_ZEPHCORE_AUTO_SHUTDOWN_MILLIVOLTS) && \
	CONFIG_ZEPHCORE_AUTO_SHUTDOWN_MILLIVOLTS > 0
#define ZEPHCORE_SERVER_AUTO_SHUTDOWN 1
#endif
#endif

class PowerPolicy {
public:
	/* Below this no Li-ion cell is running the node: no battery, not a low one. */
	static constexpr uint16_t NO_CELL_MV = 2000;

	struct Hooks {
		/* An app should hear about a low-battery shutdown now: send the notice
		 * live and return true (the power-off then waits a short grace for
		 * the app to fetch it), or return false (none connected). */
		bool (*notify_shutdown)(void);
		/* The alert threshold was crossed (once per discharge). */
		void (*battery_alert)(uint16_t mv, uint16_t threshold_mv);
		/* The alert is wanted at all (the v-contact is on). */
		bool (*alert_enabled)(void);
		/* prefs changed off the save path: coalesced write. */
		void (*prefs_dirty)(void);
	};

	PowerPolicy(mesh::MainBoard &board, NodePrefs &prefs, const Hooks &hooks)
		: _board(board), _prefs(prefs), _hooks(hooks) {}

	/* Every housekeeping tick; self-throttled to one reading per 30 s. */
	void tick();

	/* The <mv> of a set: 0 = a value in *v, 1 = not digits, 2 = not 0 or 2900-4200
	 * (the range sanitizeNodePrefs keeps; anything else would revert on reboot). */
	static int parseShutdownMv(const char *arg, long *v)
	{
		while (*arg == ' ') {
			arg++;
		}
		/* Digits only, then nothing but whitespace. */
		char *end = NULL;

		*v = strtol(arg, &end, 10);
		while (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t') {
			end++;
		}
		if (arg[0] < '0' || arg[0] > '9' || *end != '\0') {
			return 1;
		}
		return (*v != 0 && (*v < 2900 || *v > 4200)) ? 2 : 0;
	}

	/* `get|set autoshutdown` on the prefs alone; true if the line was one.
	 * *changed is set when a set stored a value (the caller saves). */
	static bool autoShutdownCommand(NodePrefs &prefs, const char *line, char *reply,
					size_t cap, bool *changed)
	{
		if (strcmp(line, "get autoshutdown") == 0) {
			if (prefs.auto_shutdown_mv == 0) {
				snprintf(reply, cap, "autoshutdown: off");
			} else {
				snprintf(reply, cap, "autoshutdown: %u mV", prefs.auto_shutdown_mv);
			}
			return true;
		}
		if (strncmp(line, "set autoshutdown ", 17) != 0) {
			return false;
		}
		long v;
		const int bad = parseShutdownMv(line + 17, &v);

		if (bad == 1) {
			snprintf(reply, cap, "ERROR: numbers only (0 = off, 2900-4200 mV)");
			return true;
		}
		if (bad == 2) {
			snprintf(reply, cap, "ERROR: must be 0 (off) or 2900-4200 mV");
			return true;
		}
		prefs.auto_shutdown_mv = (uint16_t)v;
		prefs.auto_shutdown_set = 1;
		*changed = true;
		if (v == 0) {
			snprintf(reply, cap, "OK - autoshutdown off");
		} else {
			snprintf(reply, cap, "OK - autoshutdown %ld mV", v);
		}
		return true;
	}

	/* The same setting as `get|set z.autoshutdown`, for the roles that reach it
	 * through CommonCLI. */
	static bool zAutoShutdownCommand(NodePrefs &prefs, const char *line, char *reply,
					 size_t cap, bool *changed)
	{
		if (strcmp(line, "get z.autoshutdown") == 0) {
			if (prefs.auto_shutdown_mv == 0) {
				snprintf(reply, cap, "> off");
			} else {
				snprintf(reply, cap, "> %u mV", prefs.auto_shutdown_mv);
			}
			return true;
		}
		if (strncmp(line, "set z.autoshutdown ", 19) != 0) {
			return false;
		}
		long v;

		if (parseShutdownMv(line + 19, &v) != 0) {
			snprintf(reply, cap, "Error: 0 (off) or 2900-4200 mV");
			return true;
		}
		prefs.auto_shutdown_mv = (uint16_t)v;
		prefs.auto_shutdown_set = 1;
		*changed = true;
		snprintf(reply, cap, "OK");
		return true;
	}

	/* `get|set autoshutdown`; true if the line was one. */
	bool handleCommand(const char *line, char *reply, size_t cap);

	/* The alert threshold in mV, 0 = off. The 0xFFFF default is 200 mV above
	 * the auto-shutdown cutoff, so the alert comes before the shutdown; 3500 mV
	 * without a cutoff. */
	uint16_t alertThresholdMv() const;
	/* The alert threshold changed: arm it again. */
	void rearmAlert()
	{
		_alert_latched = false;
		_alert_low = 0;
	}

private:
	void checkAlert(uint16_t mv, bool on_battery);
	void checkShutdown(uint16_t mv, bool on_battery);

	mesh::MainBoard &_board;
	NodePrefs &_prefs;
	Hooks _hooks;
	uint32_t _next_check_ms = 0;
	bool _checked = false;
	uint8_t _alert_low = 0;
	bool _alert_latched = false;
	uint8_t _shutdown_low = 0;
	bool _shutting_down = false;
};
