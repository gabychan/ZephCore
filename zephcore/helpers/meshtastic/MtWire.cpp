/*
 * SPDX-License-Identifier: MIT
 * Meshtastic frame header, channel cipher and protobuf codec. See MtWire.h.
 */

#include "MtWire.h"

#include <string.h>

namespace mt {

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

bool parseHeader(const uint8_t *raw, size_t len, Header &h)
{
	if (len < kHeaderLen) {
		return false;
	}
	h.to = rd32(&raw[0]);
	h.from = rd32(&raw[4]);
	h.id = rd32(&raw[8]);
	h.flags = raw[12];
	h.channel = raw[13];
	h.next_hop = raw[14];
	h.relay = raw[15];
	return true;
}

void writeHeader(const Header &h, uint8_t out[kHeaderLen])
{
	wr32(&out[0], h.to);
	wr32(&out[4], h.from);
	wr32(&out[8], h.id);
	out[12] = h.flags;
	out[13] = h.channel;
	out[14] = h.next_hop;
	out[15] = h.relay;
}

void ctrCrypt(AesBlockFn aes, const uint8_t key[16], uint32_t from, uint32_t id,
	      uint8_t *data, size_t len)
{
	uint8_t ctr[16] = {0};
	uint8_t ks[16];

	wr32(&ctr[0], id);  /* the upper four bytes of the 64 bit id stay zero */
	wr32(&ctr[8], from);

	while (len > 0) {
		aes(key, ctr, ks);

		size_t n = len < 16 ? len : 16;

		for (size_t i = 0; i < n; i++) {
			data[i] ^= ks[i];
		}
		data += n;
		len -= n;

		for (int i = 15; i >= 12; i--) {  /* big endian increment of the counter */
			if (++ctr[i] != 0) {
				break;
			}
		}
	}
}

static const uint8_t kDefaultKey[16] = {
	0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
	0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
};

bool expandPsk(const uint8_t *psk, size_t psk_len, uint8_t key[16], bool &encrypted)
{
	if (psk_len == 1) {
		if (psk[0] == 0) {
			memset(key, 0, 16);
			encrypted = false;
			return true;
		}
		memcpy(key, kDefaultKey, 16);
		key[15] = (uint8_t)(key[15] + psk[0] - 1);
		encrypted = true;
		return true;
	}
	if (psk_len == 16) {
		memcpy(key, psk, 16);
		encrypted = true;
		return true;
	}
	return false;
}

uint8_t channelHash(const char *name, const uint8_t key[16], bool encrypted)
{
	uint8_t h = 0;

	for (const char *c = name; *c; c++) {
		h ^= (uint8_t)*c;
	}
	if (encrypted) {
		for (int i = 0; i < 16; i++) {
			h ^= key[i];
		}
	}
	return h;
}

/* ── protobuf wire format ─────────────────────────────────────────────── */

namespace {

enum { WT_VARINT = 0, WT_I64 = 1, WT_LEN = 2, WT_I32 = 5 };

struct Reader {
	const uint8_t *p;
	const uint8_t *end;

	bool varint(uint64_t &v)
	{
		v = 0;
		for (int shift = 0; shift < 70; shift += 7) {
			if (p >= end) {
				return false;
			}
			uint8_t b = *p++;

			v |= (uint64_t)(b & 0x7F) << shift;
			if (!(b & 0x80)) {
				return true;
			}
		}
		return false;
	}

	bool fixed32(uint32_t &v)
	{
		if (end - p < 4) {
			return false;
		}
		v = rd32(p);
		p += 4;
		return true;
	}

	bool bytes(const uint8_t *&data, size_t &n)
	{
		uint64_t l;

		if (!varint(l) || l > (uint64_t)(end - p)) {
			return false;
		}
		data = p;
		n = (size_t)l;
		p += n;
		return true;
	}

	/* Next field header; false at a clean end of buffer. */
	bool field(uint32_t &num, uint32_t &wt, bool &error)
	{
		if (p >= end) {
			return false;
		}
		uint64_t tag;

		if (!varint(tag) || (tag >> 3) == 0 || (tag >> 3) > 0x1FFFFFFF) {
			error = true;
			return false;
		}
		num = (uint32_t)(tag >> 3);
		wt = (uint32_t)(tag & 7);
		return true;
	}

	bool skip(uint32_t wt)
	{
		uint64_t v;
		const uint8_t *d;
		size_t n;

		switch (wt) {
		case WT_VARINT:
			return varint(v);
		case WT_I64:
			if (end - p < 8) {
				return false;
			}
			p += 8;
			return true;
		case WT_LEN:
			return bytes(d, n);
		case WT_I32:
			if (end - p < 4) {
				return false;
			}
			p += 4;
			return true;
		default:  /* groups and the reserved types are never valid here */
			return false;
		}
	}
};

struct Writer {
	uint8_t *p;
	uint8_t *end;
	bool ok = true;

	void put(uint8_t b)
	{
		if (p >= end) {
			ok = false;
			return;
		}
		*p++ = b;
	}

	void varint(uint64_t v)
	{
		while (v >= 0x80) {
			put((uint8_t)(v | 0x80));
			v >>= 7;
		}
		put((uint8_t)v);
	}

	void tag(uint32_t num, uint32_t wt) { varint(((uint64_t)num << 3) | wt); }

	void fixed32(uint32_t num, uint32_t v)
	{
		tag(num, WT_I32);
		for (int i = 0; i < 4; i++) {
			put((uint8_t)(v >> (8 * i)));
		}
	}

	void uint(uint32_t num, uint64_t v)
	{
		tag(num, WT_VARINT);
		varint(v);
	}

	void bytes(uint32_t num, const uint8_t *d, size_t n)
	{
		tag(num, WT_LEN);
		varint(n);
		if ((size_t)(end - p) < n) {
			ok = false;
			return;
		}
		memcpy(p, d, n);
		p += n;
	}
};

void copyStr(char *dst, size_t cap, const uint8_t *src, size_t n)
{
	if (n >= cap) {
		n = cap - 1;
	}
	memcpy(dst, src, n);
	dst[n] = '\0';
}

} /* namespace */

bool decodeData(const uint8_t *buf, size_t len, Data &d)
{
	Reader r{buf, buf + len};
	bool err = false;
	bool have_port = false;
	uint32_t num, wt;

	memset(&d, 0, sizeof(d));
	while (r.field(num, wt, err)) {
		uint64_t v;

		if (num == 1 && wt == WT_VARINT) {
			if (!r.varint(v)) {
				return false;
			}
			d.portnum = (uint32_t)v;
			have_port = true;
		} else if (num == 2 && wt == WT_LEN) {
			if (!r.bytes(d.payload, d.payload_len)) {
				return false;
			}
		} else if (num == 3 && wt == WT_VARINT) {
			if (!r.varint(v)) {
				return false;
			}
			d.want_response = v != 0;
		} else if ((num == 4 || num == 5 || num == 6) && wt == WT_I32) {
			uint32_t f;

			if (!r.fixed32(f)) {
				return false;
			}
			if (num == 4) {
				d.dest = f;
			} else if (num == 5) {
				d.source = f;
			} else {
				d.request_id = f;
			}
		} else if (!r.skip(wt)) {
			return false;
		}
	}
	return !err && have_port && d.portnum >= 1 && d.portnum <= 511;
}

size_t encodeData(const Data &d, uint8_t *out, size_t cap)
{
	Writer w{out, out + cap};

	w.uint(1, d.portnum);
	if (d.payload_len > 0) {
		w.bytes(2, d.payload, d.payload_len);
	}
	if (d.want_response) {
		w.uint(3, 1);
	}
	if (d.dest) {
		w.fixed32(4, d.dest);
	}
	if (d.source) {
		w.fixed32(5, d.source);
	}
	if (d.request_id) {
		w.fixed32(6, d.request_id);
	}
	return w.ok ? (size_t)(w.p - out) : 0;
}

bool decodePosition(const uint8_t *buf, size_t len, Position &p)
{
	Reader r{buf, buf + len};
	bool err = false;
	uint32_t num, wt;

	memset(&p, 0, sizeof(p));
	while (r.field(num, wt, err)) {
		uint64_t v;
		uint32_t f;

		if ((num == 1 || num == 2) && wt == WT_I32) {
			if (!r.fixed32(f)) {
				return false;
			}
			if (num == 1) {
				p.lat_i = (int32_t)f;
				p.has_lat = true;
			} else {
				p.lon_i = (int32_t)f;
				p.has_lon = true;
			}
		} else if (num == 3 && wt == WT_VARINT) {
			if (!r.varint(v)) {
				return false;
			}
			p.altitude = (int32_t)v;
			p.has_altitude = true;
		} else if (num == 4 && wt == WT_I32) {
			if (!r.fixed32(p.time)) {
				return false;
			}
		} else if (!r.skip(wt)) {
			return false;
		}
	}
	return !err;
}

size_t encodePosition(const Position &p, uint8_t *out, size_t cap)
{
	Writer w{out, out + cap};

	if (p.has_lat) {
		w.fixed32(1, (uint32_t)p.lat_i);
	}
	if (p.has_lon) {
		w.fixed32(2, (uint32_t)p.lon_i);
	}
	if (p.has_altitude) {
		w.uint(3, (uint64_t)(int64_t)p.altitude);
	}
	if (p.time) {
		w.fixed32(4, p.time);
	}
	return w.ok ? (size_t)(w.p - out) : 0;
}

bool decodeUser(const uint8_t *buf, size_t len, User &u)
{
	Reader r{buf, buf + len};
	bool err = false;
	uint32_t num, wt;

	memset(&u, 0, sizeof(u));
	while (r.field(num, wt, err)) {
		uint64_t v;
		const uint8_t *d;
		size_t n;

		if ((num == 1 || num == 2 || num == 3) && wt == WT_LEN) {
			if (!r.bytes(d, n)) {
				return false;
			}
			if (num == 1) {
				copyStr(u.id, sizeof(u.id), d, n);
			} else if (num == 2) {
				copyStr(u.long_name, sizeof(u.long_name), d, n);
			} else {
				copyStr(u.short_name, sizeof(u.short_name), d, n);
			}
		} else if ((num == 5 || num == 7) && wt == WT_VARINT) {
			if (!r.varint(v)) {
				return false;
			}
			if (num == 5) {
				u.hw_model = (uint32_t)v;
			} else {
				u.role = (uint32_t)v;
			}
		} else if (!r.skip(wt)) {
			return false;
		}
	}
	return !err;
}

size_t encodeUser(const User &u, uint8_t *out, size_t cap)
{
	Writer w{out, out + cap};

	w.bytes(1, (const uint8_t *)u.id, strlen(u.id));
	w.bytes(2, (const uint8_t *)u.long_name, strlen(u.long_name));
	w.bytes(3, (const uint8_t *)u.short_name, strlen(u.short_name));
	if (u.hw_model) {
		w.uint(5, u.hw_model);
	}
	if (u.role) {
		w.uint(7, u.role);
	}
	return w.ok ? (size_t)(w.p - out) : 0;
}

} /* namespace mt */
