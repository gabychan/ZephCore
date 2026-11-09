/*
 * SPDX-License-Identifier: MIT
 * Meshtastic PKI direct messages: X25519 key agreement, SHA-256 of the shared
 * secret as the AES-256 key, AES-CCM (8 byte tag, 2 byte length, no AAD).
 * Pure code: the AES-256 block, SHA-256 and the random word are injected.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mt {

/* Bytes a sealed payload adds: tag (8) and extra nonce (4). */
static constexpr size_t kPkiOverhead = 12;

typedef void (*Aes256BlockFn)(const uint8_t key[32], const uint8_t in[16], uint8_t out[16]);
typedef void (*Sha256Fn)(const uint8_t *msg, size_t len, uint8_t out[32]);

struct PkiCrypto {
	Aes256BlockFn aes;
	Sha256Fn sha256;
	uint32_t (*random32)();
};

/* Private key from 32 bytes of the node identity, domain separated:
 * SHA256("zt-mtpki" || seed). */
void pkiPrivateKey(const PkiCrypto &c, const uint8_t seed[32], uint8_t priv[32]);
void pkiPublicKey(const uint8_t priv[32], uint8_t pub[32]);

/* AES-256 key of a peer: SHA256(X25519(priv, peer_pub)). False for a weak
 * peer key (all-zero shared secret). */
bool pkiSharedKey(const PkiCrypto &c, const uint8_t priv[32], const uint8_t peer_pub[32],
		  uint8_t key[32]);

/* cipher || tag || extra nonce into out (len + kPkiOverhead bytes); the nonce is
 * id | extra | from | 0. Bytes written, 0 when cap is too small. */
size_t pkiSeal(const PkiCrypto &c, const uint8_t key[32], uint32_t from, uint32_t id,
	       const uint8_t *plain, size_t len, uint8_t *out, size_t cap);

/* Plain text of a sealed payload into out (len - kPkiOverhead bytes). Bytes
 * written, or -1 when the tag does not match. */
int pkiOpen(const PkiCrypto &c, const uint8_t key[32], uint32_t from, uint32_t id,
	    const uint8_t *in, size_t len, uint8_t *out, size_t cap);

} /* namespace mt */
