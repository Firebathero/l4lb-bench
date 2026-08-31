/* Build shim, not upstream code.
 *
 * dpvs ships src/ipvs/libconhash/configure.h with `#include "dpdk.h"`, and
 * conhash.c / conhash_inter.c allocate with rte_zmalloc / rte_free. Those are
 * the ONLY DPDK dependencies in the library; the rbtree, the md5 hashing and
 * the virtual-node placement are pure C.
 *
 * Rather than patch dpvs source, this header is placed on the include path so
 * the quoted `#include "dpdk.h"` resolves here. Every libconhash .c and .h file
 * is compiled byte-for-byte as shipped.
 *
 * Substituting malloc for rte_zmalloc changes where the nodes live. It does not
 * change which node a key maps to, so disruption and balance results are
 * unaffected. Lookup cost is affected only through allocator locality, and
 * allocation happens at table-build time, not on the lookup path.
 */
#pragma once

/* stdio is required: conhash_inter.c:50 calls snprintf but includes no stdio
 * header of its own. dpvs's real dpdk.h pulls it in transitively, so without
 * it here the file compiles with an implicit declaration. Supplying it keeps
 * the compilation environment equivalent to dpvs's own. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTE_CACHE_LINE_SIZE 64

static inline void *rte_zmalloc(const char *type, size_t size, unsigned align) {
  (void)type;
  void *p = NULL;
  if (align < sizeof(void *))
    align = sizeof(void *);
  /* round size up to a multiple of align, required by aligned_alloc */
  size_t rounded = ((size + align - 1) / align) * align;
  p = aligned_alloc(align, rounded);
  if (p)
    memset(p, 0, rounded);
  return p;
}

static inline void rte_free(void *p) { free(p); }
