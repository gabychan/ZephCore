/*
 * SPDX-License-Identifier: MIT
 * Meshtastic PKI direct messages. See MtPki.h.
 */

#include "MtPki.h"

#include <string.h>

#include <monocypher.h>

namespace mt {

static constexpr size_t kTag = 8;
static constexpr size_t kNonce = 13;

static void putLe32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static uint32_t getLe32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* id | extra nonce | from | 0 */
static void buildNonce(uint32_t id, uint32_t extra, uint32_t from, uint8_t nonce[kNonce])
{
	putLe32(nonce, id);
	putLe32(nonce + 4, extra);
	putLe32(nonce + 8, from);
	nonce[12] = 0;
}

static void aesBlock(const PkiCrypto &c, const uint8_t key[32], const uint8_t in[16], uint8_t out[16])
{
	uint8_t tmp[16];

	c.aes(key, in, tmp);
	memcpy(out, tmp, 16);
}

/* RFC 3610 CBC-MAC over the message, M = 8, L = 2, no AAD. */
static void ccmTag(const PkiCrypto &c, const uint8_t key[32], const uint8_t nonce[kNonce],
		   const uint8_t *msg, size_t len, uint8_t tag[16])
{
	uint8_t x[16] = {};

	x[0] = (uint8_t)((((kTag - 2) / 2) << 3) | 1);
	memcpy(x + 1, nonce, kNonce);
	x[14] = (uint8_t)(len >> 8);
	x[15] = (uint8_t)len;
	aesBlock(c, key, x, x);
	for (size_t off = 0; off < len; off += 16) {
		size_t n = len - off < 16 ? len - off : 16;

		for (size_t i = 0; i < n; i++) {
			x[i] ^= msg[off + i];
		}
		aesBlock(c, key, x, x);
	}
	memcpy(tag, x, 16);
}

/* CTR keystream of RFC 3610: block i is AES(flags 1 | nonce | i, 2 bytes big endian);
 * block 0 masks the tag, the message starts at block 1. */
static void ccmBlock(const PkiCrypto &c, const uint8_t key[32], const uint8_t nonce[kNonce],
		     uint16_t counter, uint8_t ks[16])
{
	uint8_t a[16];

	a[0] = 1;
	memcpy(a + 1, nonce, kNonce);
	a[14] = (uint8_t)(counter >> 8);
	a[15] = (uint8_t)counter;
	aesBlock(c, key, a, ks);
}

static void ccmCtr(const PkiCrypto &c, const uint8_t key[32], const uint8_t nonce[kNonce],
		   const uint8_t *in, size_t len, uint8_t *out)
{
	uint8_t ks[16];

	for (size_t off = 0; off < len; off += 16) {
		size_t n = len - off < 16 ? len - off : 16;

		ccmBlock(c, key, nonce, (uint16_t)(off / 16 + 1), ks);
		for (size_t i = 0; i < n; i++) {
			out[off + i] = (uint8_t)(in[off + i] ^ ks[i]);
		}
	}
}

void pkiPrivateKey(const PkiCrypto &c, const uint8_t seed[32], uint8_t priv[32])
{
	static const char kLabel[] = "zt-mtpki";
	uint8_t msg[sizeof(kLabel) - 1 + 32];

	memcpy(msg, kLabel, sizeof(kLabel) - 1);
	memcpy(msg + sizeof(kLabel) - 1, seed, 32);
	c.sha256(msg, sizeof(msg), priv);
	crypto_wipe(msg, sizeof(msg));
}

void pkiPublicKey(const uint8_t priv[32], uint8_t pub[32])
{
	crypto_x25519_public_key(pub, priv);
}

bool pkiSharedKey(const PkiCrypto &c, const uint8_t priv[32], const uint8_t peer_pub[32],
		  uint8_t key[32])
{
	static const uint8_t kZero[32] = {};
	uint8_t shared[32];

	crypto_x25519(shared, priv, peer_pub);
	bool weak = crypto_verify32(shared, kZero) == 0;

	if (!weak) {
		c.sha256(shared, sizeof(shared), key);
	}
	crypto_wipe(shared, sizeof(shared));
	return !weak;
}

size_t pkiSeal(const PkiCrypto &c, const uint8_t key[32], uint32_t from, uint32_t id,
	       const uint8_t *plain, size_t len, uint8_t *out, size_t cap)
{
	uint8_t nonce[kNonce], tag[16], ks[16];
	uint32_t extra = c.random32();

	if (cap < len + kPkiOverhead) {
		return 0;
	}
	buildNonce(id, extra, from, nonce);
	ccmTag(c, key, nonce, plain, len, tag);
	ccmCtr(c, key, nonce, plain, len, out);
	ccmBlock(c, key, nonce, 0, ks);
	for (size_t i = 0; i < kTag; i++) {
		out[len + i] = (uint8_t)(tag[i] ^ ks[i]);
	}
	putLe32(out + len + kTag, extra);
	return len + kPkiOverhead;
}

int pkiOpen(const PkiCrypto &c, const uint8_t key[32], uint32_t from, uint32_t id,
	    const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
	uint8_t nonce[kNonce], tag[16], ks[16];

	if (len <= kPkiOverhead || cap < len - kPkiOverhead) {
		return -1;
	}
	size_t n = len - kPkiOverhead;

	buildNonce(id, getLe32(in + n + kTag), from, nonce);
	ccmCtr(c, key, nonce, in, n, out);
	ccmTag(c, key, nonce, out, n, tag);
	ccmBlock(c, key, nonce, 0, ks);
	for (size_t i = 0; i < kTag; i++) {
		tag[i] ^= ks[i];
	}
	uint8_t diff = 0;

	for (size_t i = 0; i < kTag; i++) {
		diff |= (uint8_t)(tag[i] ^ in[n + i]);
	}
	if (diff != 0) {
		crypto_wipe(out, n);
		return -1;
	}
	return (int)n;
}

} /* namespace mt */
