# Consistent hashing: measured comparison

First dataset of the study. Four real implementations, compiled from `repos/` at
the SHAs in `repos.md`, driven by `harness/run/ch_bench.cpp`. Nothing was
reimplemented; the harness feeds keys in and counts what comes back.

Raw results: `notes/data/ch_bench.csv`, 55 rows.

| under test | source | table |
|---|---|---|
| `katran-maglev` | `repos/katran/katran/lib/MaglevHash.cpp` | ring of 65537 / 16381 / 4093 |
| `katran-maglev-v2` | `repos/katran/katran/lib/MaglevHashV2.cpp` | same |
| `anchorhash` | `repos/anchorhash-cpp/AnchorHashQre.cpp` | anchor = working set |
| `dpvs-conhash` | `repos/dpvs/src/ipvs/libconhash/*.c` | 160 replicas per unit weight |

Ring sizes are the three deployed defaults: **65537** is katran's
`kDefaultChRingSize` (`CHHelpers.h:25`), **16381** is cilium's
`DefaultTableSize` (`pkg/maglev/maglev.go:38`), **4093** is dpvs's
`DP_VS_MH_TAB_SIZE` (`ip_vs_mh.c:60`, `primes[4]`). Running katran's real
Maglev at all three isolates the effect of table size from the effect of
implementation.

Method: 1,000,000 deterministic keys (splitmix64, fixed seed). For each backend
count, one backend is removed, every key is re-looked-up, and the change is
counted. Repeated for up to 8 different victims and averaged. Cycles and
instructions come from the hardware PMU validated in `pmu-validation.md`.

---

## 1. Excess disruption

When one of N backends is removed, some flows must move: those that were on it,
about 1/N. Anything beyond that is a connection broken for no reason.

`excess = disruption - ideal`, at equal weights:

| backends | katran Maglev 65537 | Maglev 16381 | Maglev 4093 | AnchorHash | dpvs conhash |
|---:|---:|---:|---:|---:|---:|
| 8 | 0.368% | 0.379% | 1.066% | **0.000%** | **0.000%** |
| 16 | 0.340% | 0.727% | 1.192% | **0.000%** | **0.000%** |
| 32 | 0.398% | 0.877% | 1.942% | **0.000%** | **0.000%** |
| 64 | 0.484% | 1.012% | 2.188% | **0.000%** | **0.000%** |
| 128 | 0.583% | 1.228% | 2.803% | **0.000%** | **0.000%** |

AnchorHash and dpvs conhash are **exactly** minimally disruptive: to six decimal
places, the only keys that move are the ones that were on the removed backend.
Maglev never is, and it degrades in two directions at once: worse with more
backends, and worse with a smaller table.

As a ratio of what is unavoidable, at 128 backends:

| table | ideal | actual | connections broken vs. necessary |
|---|---:|---:|---:|
| 65537 | 0.778% | 1.361% | **1.75x** |
| 16381 | 0.784% | 2.012% | **2.57x** |
| 4093 | 0.778% | 3.581% | **4.60x** |

At dpvs's 4093-entry table with 128 backends, removing one backend resets about
4.6 times as many connections as it has to. This is the property Beamer (NSDI'18)
and Cheetah (NSDI'20) exist to remove, and it is why both papers treat
Maglev-style tables as insufficient on their own for per-connection consistency.

## 2. Balance

Peak-to-mean of the key distribution at equal weights. 1.000 is perfect.

| backends | Maglev 65537 | AnchorHash | dpvs conhash |
|---:|---:|---:|---:|
| 8 | 1.005 | 1.005 | 1.091 |
| 16 | 1.005 | 1.006 | 1.122 |
| 32 | 1.011 | 1.008 | 1.155 |
| 64 | 1.023 | 1.017 | 1.218 |
| 128 | 1.034 | 1.026 | 1.259 |

Maglev and AnchorHash stay within about 3% of even at 128 backends. dpvs conhash
is at 26% over mean, with coefficient of variation 0.080 against Maglev's 0.011.
That is the cost of 160 replicas per backend: it is a ring of 20,480 virtual
nodes at N=128, and ring placement is random rather than balanced by
construction.

## 3. Update cost

Time to apply a single backend removal, microseconds:

| backends | Maglev 65537 | Maglev 4093 | AnchorHash | dpvs conhash |
|---:|---:|---:|---:|---:|
| 8 | 928.6 | 51.9 | **0.0** | 36.7 |
| 32 | 1183.9 | 59.3 | **0.1** | 63.7 |
| 128 | 1231.4 | 54.7 | **0.1** | 65.4 |

Maglev has no incremental update. `generateHashRing` rebuilds the whole ring, so
its update cost equals its build cost, around 1.2 ms for a 65537-entry table
regardless of how small the change was. AnchorHash's `UpdateRemoval` is a
handful of array writes and is roughly **10,000x cheaper**. dpvs conhash deletes
that node's 160 virtual nodes from the rbtree, around 60 microseconds.

Build cost for conhash grows steeply with N: 431 us at 8 backends, 8,540 us at
128, and 24,863 us at 128 with skewed weights, because replica count scales with
weight.

## 4. Lookup cost

Cycles and instructions per lookup, from the PMU:

| implementation | cycles/lookup | instructions/lookup |
|---|---:|---:|
| katran Maglev (any table size) | 8.0 - 8.2 | 8.00 |
| AnchorHash | 7.1 - 7.2 | 34.00 |
| dpvs conhash | 868 - 1605 | ~2300 |

Maglev is a modulo and one array index, and it is flat across table size and
backend count. AnchorHash executes about 4x the instructions; its inner loop is
`crc32c` on SSE4.2 plus a short walk that usually terminates immediately when no
buckets have been removed.

### Measurement noise, and what it does and does not support

Five repeat runs at 300k keys, all 55 configurations:

- **instruction counts are identical in every run of every configuration.** They
  are an exact retired-instruction count, not a sample.
- **cycle counts are not.** Median run-to-run spread 1.6%, maximum **33.6%**.

So the instruction column is exact and the cycle column carries real variance.
Conclusions have to be sized accordingly:

| claim | supported? |
|---|---|
| conhash costs ~100x more per lookup than Maglev | **yes**, the gap is 1000 vs 8, far outside 34% |
| Maglev is flat across table size and backend count | **yes**, 8.00 instructions everywhere |
| AnchorHash executes ~4x Maglev's instructions | **yes**, 34.00 vs 8.00 exactly |
| AnchorHash is *cheaper in cycles* than Maglev | **no** |

That last one was claimed in an earlier draft of this file and is withdrawn.
AnchorHash measures 7.08-8.04 cycles across repeats and Maglev measures
7.99-8.67; the ranges overlap. On this host the two cannot be separated on
cycles, only on instructions. Distinguishing them would need a machine with
precise sampling and uncore counters, which `env/host.md` records this one as
lacking (`max_precise = 0`).

dpvs conhash is two orders of magnitude more expensive because every lookup is
an `snprintf`, an MD5 digest, and a red-black tree descent. The `snprintf` is
not an artefact of this harness: dpvs itself formats the key into a string
before calling `conhash_lookup` (`ip_vs_conhash.c:121,124,131`), so this is its
real path.

**Read these as algorithm-level numbers, not datapath numbers.** In katran the
ring lookup is a BPF map access inside an XDP program, not a userspace array
index. What transfers is the relative cost of the *selection* step, not the
per-packet cost of the load balancer.

## 5. MaglevHash does not implement weights; MaglevHashV2 does

The two katran variants produce byte-identical results at equal weights, which
initially looked like redundancy. Running them with skewed weights (backend `i`
gets weight `(i % 4) + 1`, so weights cycle 1,2,3,4) separates them completely:

| implementation | weights | peak-to-mean | min-to-mean | cv |
|---|---|---:|---:|---:|
| katran-maglev (V1) | skewed | 1.035 | 0.970 | 0.011 |
| katran-maglev-v2 | skewed | **1.632** | **0.385** | **0.448** |
| dpvs-conhash | skewed | 1.768 | 0.341 | 0.449 |

At 128 backends, ring 65537. V1's distribution under skewed weights is
indistinguishable from its distribution under equal weights: **it ignores the
weights**. V2 tracks them almost exactly. With weights 1,2,3,4 the mean weight
is 2.5, so a correct implementation gives peak-to-mean 4/2.5 = **1.600** and
min-to-mean 1/2.5 = **0.400**. V2 measures 1.632 and 0.385.

The mechanism is visible in the source. `MaglevHash.cpp:48-61`:

```cpp
for (int j = 0; j < endpoints[i].weight; j++) {
  ...
  result[cur] = endpoints[i].num;
  ...
}
endpoints[i].weight = 1;
```

The weight is honoured on the first pass of the outer loop and then reset to 1,
so for a 65537-entry ring only about `sum(weights)` of 65537 slots are ever
weighted. The effect is swamped. `MaglevHashV2.cpp:45-56` instead carries
cumulative weights across every pass, which is why it holds the ratio.

`CHFactory::make` defaults to `MaglevHash` for an unrecognised enum value
(`CHHelpers.cpp:24-29`). Anything relying on weighted backend selection needs
`HashFunction::MaglevV2` explicitly.

---

## What this does and does not establish

Establishes, on real implementations:

- AnchorHash and dpvs conhash are exactly minimally disruptive on backend removal; Maglev is not, by 1.75x to 4.6x
- Maglev's excess disruption worsens with smaller tables and more backends
- conhash pays for its consistency with 10-26% load imbalance and roughly 1000 cycles per lookup
- AnchorHash updates in constant time, about 10,000x faster than regenerating a Maglev ring
- katran's V1 Maglev silently ignores backend weights

Does not establish:

- anything about packets per second, latency, or datapath cost. These are userspace algorithm measurements.
- anything about cilium's Maglev implementation. It is Go and depends on `hive/cell`, `workerpool`, `pflag`, `loadbalancer`, `lock` and `murmur3`; it was not built. Only its table size (16381) was carried across, applied to katran's implementation.
- anything about dpvs's own `mh` scheduler (`ip_vs_mh.c`). It does not separate from DPDK, so only its table size (4093) was carried across.

## Invariants checked

`./ch_bench verify` asserts the properties these numbers depend on. All 23 pass.

The one that matters most is **restore exactness**. Each implementation is
measured by removing a backend and putting it back, up to 8 times. If a restore
were not exact, every trial after the first would measure a drifted structure
and the averages would be quietly wrong. Verified per-key, not statistically:

- AnchorHash: `UpdateNewBucket()` reproduces the assignment vector of all
  200,000 keys exactly after `UpdateRemoval()`
- dpvs conhash: re-adding the node rebuilds a ring that reproduces the
  assignment vector exactly
- katran Maglev: regeneration from the full endpoint set reproduces the original
  ring byte for byte

Also asserted: the Maglev ring is fully populated with no `-1` slots left; ring
generation is deterministic; every key is assigned to an in-range backend; no
key maps to the removed backend afterwards; every key that sat on the removed
backend actually moved; and disruption is never below ideal. For AnchorHash and
conhash the excess is checked to be **exactly** zero per key, not merely zero
after rounding.

End-to-end, two full runs produce byte-identical output in every non-timing
column, and the committed `data/ch_bench.csv` reproduces exactly from a fresh
1M-key run.

## Reproducing

```bash
cd harness/run
make            # needs g++ and gcc only
make verify     # 23 invariant checks
make run        # writes ../results/ch_bench.csv
```

Two deviations from a stock build, both disclosed in `harness/run/README.md`:
the DPDK allocator shim required to compile dpvs's libconhash outside DPDK, and
a no-op node finaliser the harness must pass to `conhash_fini`.

## Environment

Host as described in `env/host.md`. Built at `-O2 -msse4.2`.

Produced twice, on two different compilers:

| | distro | compiler |
|---|---|---|
| original run | WSL2 `Ubuntu-22.04` | g++ 11.4 |
| independent reproduction | WSL2 `Ubuntu-24.04` | g++ 13.3 |

Both produce **identical values in every deterministic column**, including the
per-lookup instruction counts. So the results are not an artefact of a
particular compiler version. The two distros share one WSL2 kernel, so this
isolates the toolchain, not the kernel.
