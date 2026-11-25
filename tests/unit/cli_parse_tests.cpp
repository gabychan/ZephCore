// helpers/cli_parse.h: parsing of the `time <unix>` argument, shared with the
// clock-set path so the host harness reaches it without a board.
#include "test.h"
#include "cli_parse.h"

namespace {

cli_parse::TimeArg ok(const char* arg)
{
    cli_parse::TimeArg r = cli_parse::parseTimeArg(arg);
    CHECK(r.status == cli_parse::TIME_ARG_OK);
    return r;
}

cli_parse::TimeArgStatus status(const char* arg)
{
    return cli_parse::parseTimeArg(arg).status;
}

}  // namespace

TEST(cli_time_decimal, "UNIT-CLIPARSE-001", "A plain decimal timestamp is accepted")
{
    CHECK(ok("1700000000").secs == 1700000000u);
    CHECK(ok("0").secs == 0u);
    // Leading zeros are decimal, not octal: strtoul's base-10 is stated, not guessed.
    CHECK(ok("0000000007").secs == 7u);
    CHECK(ok("1").secs == 1u);
}

TEST(cli_time_sign, "UNIT-CLIPARSE-002", "A leading sign is refused, because strtoul negates it")
{
    // strtoul("-1") returns ULONG_MAX with no error, and the cast to uint32_t
    // makes that 0xFFFFFFFF -- the latest possible clock, set from a typo.
    CHECK(status("-1") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status("+1700000000") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status(" -1") == cli_parse::TIME_ARG_NOT_A_NUMBER);
}

TEST(cli_time_range, "UNIT-CLIPARSE-003", "UINT32_MAX and anything above it is refused")
{
    // 0xFFFFFFFF truncates from 64-bit unsigned long on a PC build, and the next
    // unique timestamp after it wraps to 0, so it is never a usable value.
    CHECK(status("4294967295") == cli_parse::TIME_ARG_OUT_OF_RANGE);
    CHECK(status("4294967296") == cli_parse::TIME_ARG_OUT_OF_RANGE);
    CHECK(status("99999999999999999999") == cli_parse::TIME_ARG_OUT_OF_RANGE);
    // The largest value that is still usable.
    CHECK(ok("4294967294").secs == 4294967294u);
}

TEST(cli_time_junk, "UNIT-CLIPARSE-004", "Empty input, trailing junk and embedded signs are refused")
{
    CHECK(status("") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status("   ") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status("1700000000x") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status("1700000000 ") == cli_parse::TIME_ARG_OK);      // trailing space is fine
    CHECK(status("17 00000000") == cli_parse::TIME_ARG_NOT_A_NUMBER);
    CHECK(status("0x10") == cli_parse::TIME_ARG_NOT_A_NUMBER);     // decimal only, no hex
}
