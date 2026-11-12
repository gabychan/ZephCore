/*
 * SPDX-License-Identifier: MIT
 * What the dashboard page shows, side by side for MeshCore and Meshtastic.
 * The mesh fills it on the main thread; the page only draws it.
 */

#ifndef ZEPHCORE_UI_DASH_H
#define ZEPHCORE_UI_DASH_H

#include <stdbool.h>
#include <stdint.h>

#define UI_DASH_NODES 3

struct ui_dash_node {
	char name[15];   /* "" : no node in this row */
	int8_t snr_x4;
	uint32_t age_s;
};

struct ui_dash {
	uint32_t mc_rx, mc_tx, mc_err;
	uint32_t mc_flood_tx, mc_flood_rx, mc_direct_tx, mc_direct_rx;
	uint32_t air_tx_s, air_rx_s;
	bool mt_on;                  /* the Meshtastic side is built in and enabled */
	uint32_t mt_heard, mt_relayed, mt_dropped;
	char mt_cfg[20];             /* "client relay off" */
	struct ui_dash_node mc_n[UI_DASH_NODES], mt_n[UI_DASH_NODES];
	uint8_t mc_n_total, mt_n_total;  /* nodes heard in all, not only those listed */
	uint32_t heap_used, heap_total;  /* bytes; total 0: not counted */
	uint32_t flash_used_kb, flash_total_kb;
	uint32_t uptime_s;
};

#endif /* ZEPHCORE_UI_DASH_H */
