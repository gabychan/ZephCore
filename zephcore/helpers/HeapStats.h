/*
 * SPDX-License-Identifier: MIT
 * What the kernel heaps hold now and at their peak, for "get z.info" and the
 * dashboard screen. All zero without CONFIG_SYS_HEAP_RUNTIME_STATS.
 */

#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>

struct HeapTotals {
	uint32_t used, total, peak;  /* bytes */
};

inline HeapTotals heapTotals()
{
	HeapTotals t = {};

#if IS_ENABLED(CONFIG_SYS_HEAP_RUNTIME_STATS)
	STRUCT_SECTION_FOREACH(k_heap, heap) {
		struct sys_memory_stats st;

		sys_heap_runtime_stats_get(&heap->heap, &st);
		t.used += st.allocated_bytes;
		t.total += st.allocated_bytes + st.free_bytes;
		t.peak += st.max_allocated_bytes;
	}
#endif
	return t;
}
