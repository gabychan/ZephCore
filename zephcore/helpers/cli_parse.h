/*
 * SPDX-License-Identifier: MIT
 * cli_parse - Argument parsing for CLI commands, kept out of the Zephyr layer
 * so the host harness can test it.
 */
#pragma once

#include <stdint.h>

namespace cli_parse {

enum TimeArgStatus {
	TIME_ARG_OK,
	TIME_ARG_NOT_A_NUMBER,  // empty, leading sign, or trailing junk
	TIME_ARG_OUT_OF_RANGE,  // numeric but not a usable clock value
};

struct TimeArg {
	TimeArgStatus status;
	uint32_t secs;  // valid only when status == TIME_ARG_OK
};

/* Parses the argument of `time <unix>`: a decimal Unix timestamp that fits a
 * uint32_t. Stricter than strtoul on purpose:
 *   - a leading sign is refused, because strtoul("-1") returns ULONG_MAX with
 *     no error and the cast would set the clock to 0xFFFFFFFF;
 *   - the value must stay below UINT32_MAX, because on a 64-bit host the cast
 *     truncates silently and the next unique timestamp after 0xFFFFFFFF wraps
 *     to 0.
 * Surrounding whitespace is allowed; anything else after the digits is not. */
TimeArg parseTimeArg(const char* arg);

}  // namespace cli_parse
