/*
 * SPDX-License-Identifier: MIT
 */
#include "cli_parse.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

namespace cli_parse {
namespace {

const char* skipSpace(const char* s)
{
	while (*s == ' ' || *s == '	') s++;
	return s;
}

}  // namespace

TimeArg parseTimeArg(const char* arg)
{
	if (arg == nullptr) return TimeArg{TIME_ARG_NOT_A_NUMBER, 0};

	const char* start = skipSpace(arg);
	// Decimal digits only: this also refuses the leading sign that strtoul
	// would otherwise accept as a negation.
	if (*start < '0' || *start > '9') return TimeArg{TIME_ARG_NOT_A_NUMBER, 0};

	char* end = nullptr;
	errno = 0;
	unsigned long parsed = strtoul(start, &end, 10);

	if (end == start || *skipSpace(end) != '\0') return TimeArg{TIME_ARG_NOT_A_NUMBER, 0};
	// ERANGE covers an input too large for unsigned long at all.
	if (errno == ERANGE || parsed >= UINT32_MAX) return TimeArg{TIME_ARG_OUT_OF_RANGE, 0};

	return TimeArg{TIME_ARG_OK, (uint32_t)parsed};
}

}  // namespace cli_parse
