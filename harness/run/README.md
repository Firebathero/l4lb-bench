# harness/run

## What this does

`ch_bench.cpp` measures four real consistent-hashing implementations against
each other by compiling them from `repos/` and calling their own APIs. It does
not reimplement any algorithm.

## What you run

```bash
make            # needs g++ and gcc, nothing else
make verify     # 23 invariant checks, exits non-zero on failure
make run        # writes ../results/ch_bench.csv
./ch_bench 1000000        # key count, default 1000000
./ch_bench verify 200000  # invariant checks at a chosen key count
```

`make verify` is the one to run after changing anything. It checks per-key, not
statistically, that removals and restores are exact, that no key is left
unassigned or pointing at a removed backend, and that disruption never falls
below the unavoidable minimum. A contaminated restore would silently corrupt
every trial after the first.

Results committed at `../../notes/data/ch_bench.csv`, analysis in
`../../notes/consistent-hashing.md`.

## What you get

CSV, one row per (implementation, weighting, backend count, table size):

| column | meaning |
|---|---|
| `disruption` | fraction of keys that change backend when one backend is removed |
| `ideal` | fraction that sat on the removed backend, the unavoidable minimum |
| `excess` | `disruption - ideal`, connections broken for no reason |
| `peak_to_mean`, `min_to_mean`, `cv` | load distribution across backends |
| `build_us` | build the lookup structure from scratch |
| `update_us` | apply one backend removal |
| `cycles_per_lookup`, `ins_per_lookup` | hardware PMU, see `../../notes/pmu-validation.md` |
| `ns_per_lookup` | wall clock |

## What you can change

- key count: `./ch_bench <N>`
- backend counts and ring sizes: `backend_counts[]` and `ring_sizes[]` in `main()`
- weighting: `weight_of()`, currently `(i % 4) + 1` for the skewed case
- key distribution: `splitmix64(KEY_SEED + i)`, deterministic and seeded

## Sources compiled

Paths are relative to the repo root, at the SHAs in `../../notes/repos.md`.

```
repos/katran/katran/lib/CHHelpers.cpp
repos/katran/katran/lib/MaglevBase.cpp
repos/katran/katran/lib/MaglevHash.cpp
repos/katran/katran/lib/MaglevHashV2.cpp
repos/katran/katran/lib/MurmurHash3.cpp
repos/anchorhash-cpp/AnchorHashQre.cpp
repos/dpvs/src/ipvs/libconhash/{conhash,conhash_inter,conhash_util,md5,util_rbtree}.c
```

Every one of those is compiled byte-for-byte as shipped. No upstream file is
patched.

## Two deviations, disclosed

**1. `shim/dpdk.h`.** dpvs's `libconhash/configure.h` does `#include "dpdk.h"`,
and `conhash.c` / `conhash_inter.c` allocate with `rte_zmalloc` / `rte_free`.
Those are the only DPDK dependencies in the library. Rather than patch dpvs, a
shim header is placed on the include path so the quoted include resolves to it.
It supplies `rte_zmalloc`, `rte_free`, `RTE_CACHE_LINE_SIZE`, and `<stdio.h>`.

`<stdio.h>` is required because `conhash_inter.c:50` calls `snprintf` while
including no stdio header of its own; dpvs's real `dpdk.h` pulls it in
transitively, and without it the file compiles with an implicit declaration.

Swapping the allocator changes where nodes live. It does not change which node a
key maps to, so the disruption and balance results are unaffected. Lookup cost
is touched only through allocator locality, and allocation happens at build
time, not on the lookup path.

**2. `node_fini_noop`.** `conhash_fini` calls `(*node_fini)(node)` once a node's
replica count reaches zero (`conhash.c:47`). Passing `NULL` jumps to address 0.
dpvs passes a finaliser that frees its own node wrapper; the harness owns its
nodes in a `std::vector`, so the correct finaliser here does nothing.

## Notes on comparability

- Each implementation applies its own key hashing. The harness supplies one
  uniform 64-bit key per flow and lets each do what it does: Maglev takes it
  modulo the ring size, AnchorHash runs its own `crc32c`, conhash formats it to
  a string and MD5s it. That is the deployed behaviour of each.
- conhash's `snprintf` is inside the measured loop because dpvs also formats the
  key to a string before calling `conhash_lookup` (`ip_vs_conhash.c:121,124,131`).
- These are userspace algorithm measurements. In katran the equivalent lookup is
  a BPF map access inside XDP. Relative selection cost transfers; absolute
  per-packet cost does not.
