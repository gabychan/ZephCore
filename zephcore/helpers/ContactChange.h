/*
 * SPDX-License-Identifier: MIT
 * What makes a heard advert (MeshCore or Meshtastic) a change worth writing
 * the contacts file for. Pure, host tested.
 */

#pragma once

#include <string.h>

#include "ContactInfo.h"

/* The record as it was before the advert, to compare with the one after. */
struct ContactSnapshot {
	bool known;
	uint8_t type;
	int32_t gps_lat, gps_lon;
	char name[sizeof(ContactInfo::name)];
};

/* A new record, or a changed name, type or position: not a bare re-advert. */
inline bool contactChangedSubstantively(const ContactSnapshot &before, const ContactInfo &after)
{
	return !before.known || before.type != after.type || before.gps_lat != after.gps_lat ||
	       before.gps_lon != after.gps_lon ||
	       strncmp(before.name, after.name, sizeof(before.name)) != 0;
}
