/*
 * SPDX-License-Identifier: MIT
 * The auto-shutdown threshold commands: `get|set autoshutdown` (the companion's
 * CLI) and `get|set z.autoshutdown` (every role, through CommonCLI), both on
 * prefs.auto_shutdown_mv. A set takes 0 (off) or AUTO_SHUTDOWN_MIN_MV to the
 * build's ceiling, digits only, so what is stored is what sanitizeNodePrefs()
 * keeps after a reboot. Pure code, host tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NodePrefs.h"

namespace autoshutdown {

enum class Parse : uint8_t { OK, NOT_DIGITS, OUT_OF_RANGE };

/* The <mv> of a set: digits, then nothing but whitespace. */
inline Parse parseMv(const char *arg, uint16_t ceiling_mv, uint16_t *mv)
{
	while (*arg == ' ') {
		arg++;
	}
	if (*arg < '0' || *arg > '9') {
		return Parse::NOT_DIGITS;
	}
	char *end = NULL;
	long v = strtol(arg, &end, 10);

	while (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t') {
		end++;
	}
	if (*end != '\0') {
		return Parse::NOT_DIGITS;
	}
	if (v > AUTO_SHUTDOWN_MAX_MV || !auto_shutdown_mv_valid((uint16_t)v, ceiling_mv)) {
		return Parse::OUT_OF_RANGE;
	}
	*mv = (uint16_t)v;
	return Parse::OK;
}

/* Stores a parsed value; *changed tells the caller to save. */
inline void store(NodePrefs &prefs, uint16_t mv, bool *changed)
{
	prefs.auto_shutdown_mv = mv;
	prefs.auto_shutdown_set = 1;
	*changed = true;
}

/* `get|set autoshutdown`; true if the line was one. */
inline bool command(NodePrefs &prefs, const char *line, uint16_t ceiling_mv, char *reply,
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
	uint16_t mv = 0;

	switch (parseMv(line + 17, ceiling_mv, &mv)) {
	case Parse::NOT_DIGITS:
		snprintf(reply, cap, "ERROR: numbers only (0 = off, %u-%u mV)",
			 AUTO_SHUTDOWN_MIN_MV, ceiling_mv);
		return true;
	case Parse::OUT_OF_RANGE:
		snprintf(reply, cap, "ERROR: must be 0 (off) or %u-%u mV", AUTO_SHUTDOWN_MIN_MV,
			 ceiling_mv);
		return true;
	case Parse::OK:
		break;
	}
	store(prefs, mv, changed);
	if (mv == 0) {
		snprintf(reply, cap, "OK - autoshutdown off");
	} else {
		snprintf(reply, cap, "OK - autoshutdown %u mV", mv);
	}
	return true;
}

/* `get|set autoshutdown` (the plain commands, any role through CommonCLI). */
inline bool isCommand(const char *line)
{
	return strcmp(line, "get autoshutdown") == 0 ||
	       strncmp(line, "set autoshutdown ", 17) == 0;
}

inline bool isZCommand(const char *line)
{
	return strcmp(line, "get z.autoshutdown") == 0 ||
	       strncmp(line, "set z.autoshutdown ", 19) == 0;
}

/* `get|set z.autoshutdown`, in the z. replies; true if the line was one. */
inline bool zCommand(NodePrefs &prefs, const char *line, uint16_t ceiling_mv, char *reply,
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
	uint16_t mv = 0;

	if (parseMv(line + 19, ceiling_mv, &mv) != Parse::OK) {
		snprintf(reply, cap, "Error: 0 (off) or %u-%u mV", AUTO_SHUTDOWN_MIN_MV, ceiling_mv);
		return true;
	}
	store(prefs, mv, changed);
	snprintf(reply, cap, "OK");
	return true;
}

}  // namespace autoshutdown
