// helpers/cli_reply.h: how many bytes a CLI handler may write, local or remote.
// Pins the invariant behind T8 and CONN-2: a remote caller must never be sized
// with the local buffer, and must always be sized down by its header.
#include "test.h"
#include "cli_reply.h"

TEST(cli_reply_local, "UNIT-CLIREPLY-001", "A local caller gets the whole reply buffer")
{
    // sender_timestamp 0 is the local console: it owns the buffer outright.
    CHECK(cli_reply::capFor(0, 0) == cli_reply::LOCAL_REPLY_SIZE);
    // A header consumes part of it even locally.
    CHECK(cli_reply::capFor(0, 7) == cli_reply::LOCAL_REPLY_SIZE - 7u);
}

TEST(cli_reply_remote, "UNIT-CLIREPLY-002", "A remote caller gets the smaller buffer, minus its header")
{
    // This is the CONN-2 defect: sizing a remote caller with the local 256 wrote
    // past the 161-byte buffer onPeerDataRecv actually hands over.
    CHECK(cli_reply::capFor(1, 0) == cli_reply::REMOTE_REPLY_SIZE);
    CHECK(cli_reply::capFor(1, 0) < cli_reply::LOCAL_REPLY_SIZE);
    CHECK(cli_reply::capFor(1700000000u, 5) == cli_reply::REMOTE_REPLY_SIZE - 5u);
}

TEST(cli_reply_sizes, "UNIT-CLIREPLY-003", "The remote buffer is smaller than the local one, on purpose")
{
    // The two sizes existing separately is the whole point: if someone makes them
    // equal, every remote reply silently regains the overflow.
    CHECK(cli_reply::REMOTE_REPLY_SIZE < cli_reply::LOCAL_REPLY_SIZE);
    CHECK(cli_reply::REMOTE_REPLY_SIZE == 161u);
    CHECK(cli_reply::LOCAL_REPLY_SIZE == 256u);
}

TEST(cli_reply_never_negative, "UNIT-CLIREPLY-004", "A header larger than the buffer yields 0, never an underflow")
{
    // size_t underflow would hand a handler the whole address space.
    CHECK(cli_reply::capFor(1, cli_reply::REMOTE_REPLY_SIZE) == 0u);
    CHECK(cli_reply::capFor(1, (uint8_t)(cli_reply::REMOTE_REPLY_SIZE + 50)) == 0u);
    CHECK(cli_reply::capFor(0, 255) == 1u);
}
