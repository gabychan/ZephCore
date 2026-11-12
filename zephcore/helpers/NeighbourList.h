/*
 * SPDX-License-Identifier: MIT
 * Zephtacore: the nodes heard directly, for "get z.mc.neighbors" and
 * "get z.mt.neighbors". A table of the last ones heard, and the reply: a title
 * and one "name (snr, age)" line each, best SNR first. Pure code, host tested.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace neighbours {

static constexpr size_t kTableSize = 16;
static constexpr size_t kPerPage = 4;    /* lines of a reply: they fit one radio packet */
static constexpr size_t kListMax = 64;   /* most nodes a list can hold */
static constexpr size_t kNameMax = 14;   /* bytes of a name in a line */
static constexpr size_t kShortMax = 4;   /* Meshtastic short name */

/* One node heard directly. */
struct Entry {
	uint32_t id;       /* 0: free entry */
	uint32_t heard_s;  /* uptime seconds of the last frame */
	int8_t snr_x4;
	char name[kShortMax + 1];
};

class Table {
public:
	/* Updates the node or takes the entry heard longest ago. */
	void note(uint32_t id, int8_t snr_x4, uint32_t now_s)
	{
		Entry *e = &_e[0];

		for (Entry &c : _e) {
			if (c.id == id) {
				e = &c;
				break;
			}
			if (e->id != 0 && (c.id == 0 || c.heard_s < e->heard_s)) {
				e = &c;
			}
		}
		if (e->id != id) {
			*e = Entry{};
			e->id = id;
			if (_pending_id == id) {
				snprintf(e->name, sizeof(e->name), "%s", _pending_name);
				_pending_id = 0;
			}
		}
		e->snr_x4 = snr_x4;
		e->heard_s = now_s;
	}

	/* The short name of a node. One not yet in the table gets it when note() enters it. */
	void setName(uint32_t id, const char *name)
	{
		for (Entry &e : _e) {
			if (e.id == id) {
				snprintf(e.name, sizeof(e.name), "%s", name);
				return;
			}
		}
		_pending_id = id;
		snprintf(_pending_name, sizeof(_pending_name), "%s", name);
	}

	const Entry *begin() const { return _e; }
	const Entry *end() const { return _e + kTableSize; }

private:
	Entry _e[kTableSize] = {};
	uint32_t _pending_id = 0;
	char _pending_name[kShortMax + 1] = {};
};

struct Row {
	char name[kNameMax + 1];
	int8_t snr_x4;
	uint32_t age_s;
};

/* A name cut to kNameMax bytes without splitting a UTF-8 character. */
inline void setRowName(Row &r, const char *name)
{
	size_t n = strlen(name);

	if (n > kNameMax) {
		n = kNameMax;
		while (n > 0 && ((uint8_t)name[n] & 0xC0) == 0x80) {
			n--;
		}
	}
	snprintf(r.name, sizeof(r.name), "%.*s", (int)n, name);
}

/* Where the nodes come from; i runs over size() slots, a free one has no key. */
class List {
public:
	virtual ~List() {}
	virtual size_t size() const = 0;
	virtual bool key(size_t i, int8_t &snr_x4, uint32_t &age_s) const = 0;
	virtual void row(size_t i, Row &r) const = 0;
};

/* "!a1b2c3d4" for a Meshtastic node, no name known. */
inline void nodeLabel(char *out, size_t cap, uint32_t node)
{
	snprintf(out, cap, "!%08x", (unsigned)node);
}

/* 45s, 4m, 3h, 2d. */
inline void formatAge(char *out, size_t cap, uint32_t s)
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

/* 10.3 from 41 (a quarter of a dB each), rounded to a tenth. */
inline void formatSnr(char *out, size_t cap, int8_t snr_x4)
{
	int t = snr_x4 * 5;

	t = (t + (t >= 0 ? 1 : -1)) / 2;
	snprintf(out, cap, "%s%d.%d", t < 0 ? "-" : "", (t < 0 ? -t : t) / 10, (t < 0 ? -t : t) % 10);
}

/* A Table as a List; label() puts the node's name in the row. */
class TableList : public List {
public:
	TableList(const Table &t, uint32_t now_s) : _t(t), _now(now_s) {}
	size_t size() const override { return kTableSize; }
	bool key(size_t i, int8_t &snr_x4, uint32_t &age_s) const override
	{
		snr_x4 = _t.begin()[i].snr_x4;
		age_s = _now - _t.begin()[i].heard_s;
		return _t.begin()[i].id != 0;
	}
	void row(size_t i, Row &r) const override
	{
		const Entry &e = _t.begin()[i];

		r.snr_x4 = e.snr_x4;
		r.age_s = _now - e.heard_s;
		label(e, r);
	}

protected:
	virtual void label(const Entry &e, Row &r) const = 0;

private:
	const Table &_t;
	uint32_t _now;
};

/* True when a goes before b: the stronger SNR, or with recent the shorter age. */
inline bool before(bool recent, int8_t snr_a, uint32_t age_a, int8_t snr_b, uint32_t age_b)
{
	return recent ? age_a < age_b : snr_a > snr_b;
}

/* The slots in use, best first, into order (at most kListMax): strongest SNR or, with recent, heard last. */
inline size_t rank(const List &list, uint8_t *order, bool recent)
{
	size_t total = 0;

	for (size_t i = 0; i < list.size() && i < kListMax; i++) {
		int8_t snr;
		uint32_t age;

		if (!list.key(i, snr, age)) {
			continue;
		}
		size_t at = total;

		while (at > 0) {
			int8_t other;
			uint32_t other_age;

			list.key(order[at - 1], other, other_age);
			if (!before(recent, snr, age, other, other_age)) {
				break;
			}
			order[at] = order[at - 1];
			at--;
		}
		order[at] = (uint8_t)i;
		total++;
	}
	return total;
}

/* The best n nodes (strongest SNR, or with recent heard last) into rows; returns how many there are. */
inline size_t top(const List &list, Row *rows, size_t n, bool recent = false)
{
	uint8_t order[kListMax];
	const size_t total = rank(list, order, recent);

	for (size_t k = 0; k < n && k < total; k++) {
		list.row(order[k], rows[k]);
	}
	return total < n ? total : n;
}

/* "<title> (<total>) <page>/<pages>" and a line per node of that page, best SNR first or, with recent,
 * heard last first (ties in list order). A page past the last says so; "-none-" with no node. */
inline void format(char *out, size_t cap, const char *title, const List &list, size_t page, bool recent = false)
{
	uint8_t order[kListMax];
	const size_t total = rank(list, order, recent);
	if (total == 0) {
		snprintf(out, cap, "%s: -none-", title);
		return;
	}
	const size_t pages = (total + kPerPage - 1) / kPerPage;

	if (page < 1 || page > pages) {
		snprintf(out, cap, "%s (%u): page %u of %u", title, (unsigned)total, (unsigned)page, (unsigned)pages);
		return;
	}
	int n = snprintf(out, cap, "%s (%u) %u/%u%s", title, (unsigned)total, (unsigned)page, (unsigned)pages,
			 recent ? " recent" : "");

	for (size_t k = (page - 1) * kPerPage; k < total && k < page * kPerPage && n > 0 && (size_t)n < cap; k++) {
		Row r = {};
		char snr[8], age[8], line[kNameMax + 24];

		list.row(order[k], r);
		formatSnr(snr, sizeof(snr), r.snr_x4);
		formatAge(age, sizeof(age), r.age_s);
		int l = snprintf(line, sizeof(line), "\n%s (%s, %s)", r.name, snr, age);

		if (l < 0 || (size_t)n + (size_t)l >= cap) {
			break;
		}
		memcpy(out + n, line, (size_t)l + 1);
		n += l;
	}
}

} /* namespace neighbours */
