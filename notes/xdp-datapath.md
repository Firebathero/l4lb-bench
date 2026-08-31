# katran's XDP datapath: per-packet cost

First packet-level measurement of the study. katran's real XDP program, compiled
unmodified from `repos/` and executed in-kernel with `BPF_PROG_TEST_RUN`.

No NIC, no hugepages, no IOMMU, no driver binding. `env/host.md` says this host
has none of those; none are needed for this. The only requirement is root, since
`unprivileged_bpf_disabled = 2`.

Driver: `harness/run/bpf/xdp_bench.c`. Raw output: `notes/data/xdp_bench.txt`.

## What was measured

`balancer_ingress` from `repos/katran/katran/lib/bpf/balancer.bpf.c`, compiled
with clang 18.1.3 at `-O2 -target bpf`. Loaded with libbpf 1.3.0 on kernel
6.18.33.2. Struct definitions for map population come from katran's own
`balancer_structs.h`, so the ABI cannot drift.

Control-plane state was populated to the minimum that makes a packet route:
one VIP in `vip_map`, `ch_rings` filled for that VIP, and `reals` populated.
Input is a 54-byte Ethernet + IPv4 + TCP SYN addressed to the VIP.

## Results

20,000 runs per row. Times are the kernel-reported `duration`, measured around
the program execution.

| backends | action | out bytes | min ns | median ns | mean ns | p99 ns |
|---:|---|---:|---:|---:|---:|---:|
| 1 | XDP_TX | 74 | 339 | 358 | 394.6 | 753 |
| 8 | XDP_TX | 74 | 331 | 358 | 400.1 | 1635 |
| 64 | XDP_TX | 74 | 339 | 367 | 409.3 | 1671 |
| 512 | XDP_TX | 74 | 339 | 367 | 401.8 | 937 |
| **baseline** | XDP_PASS | - | 55 | **64** | 70.0 | 147 |

Baseline is a non-IP frame, which exits at `balancer.bpf.c:1177` after only the
entry bounds check and the stats bump. Subtracting it isolates parse, hash,
ring lookup, backend resolution and encapsulation from `BPF_PROG_TEST_RUN`
overhead.

**Net cost of the load-balancing path: about 294 to 303 ns per packet.**

Two things the numbers say:

- **It is flat in backend count.** 1 backend and 512 backends cost the same to
  within noise. Expected: the ring lookup is one array index regardless of how
  many backends the ring was built from. This is the datapath-level counterpart
  to the flat 8.00 instructions per lookup measured in
  `consistent-hashing.md`.
- **Output is 74 bytes for a 54-byte input, every time.** Exactly 20 bytes of
  IPIP outer header, so the packet really was encapsulated rather than dropped
  or passed. The harness asserts this and exits non-zero otherwise.

Use median and min. Mean and p99 are inflated by scheduler noise on a
virtualised host; `pmu-validation.md` records the same effect on cycle counts.

## Two traps this hit

### `repeat > 1` silently reports a number 3.7x too low

`BPF_PROG_TEST_RUN` takes a repeat count, and using it is the obvious way to
amortise syscall overhead. It is wrong for this program, because katran
encapsulates: the output of run N is the input of run N+1. An IPIP packet is
neither TCP nor UDP, and `INLINE_DECAP_IPIP` is not defined in this build, so
every run after the first falls through to `XDP_PASS` at `balancer.bpf.c:812`.

Demonstrated, not assumed:

| repeat | retval | ns/run | |
|---:|---:|---:|---|
| 1 | 3 (XDP_TX) | 5556 | load balanced |
| 2 | 2 (XDP_PASS) | 3783 | **not load balanced** |
| 10 | 2 (XDP_PASS) | 681 | **not load balanced** |
| 100 | 2 (XDP_PASS) | 98 | **not load balanced** |

At `repeat=100` the reported cost is **98 ns** against a true **~360 ns**. The
`retval` is the tell: it changes from 3 to 2. Any measurement of an
encapsulating or mutating XDP program that uses `repeat > 1` without checking
`retval` is reporting the cost of the wrong code path.

So every row above uses `repeat = 1` with the loop in userspace, which costs
syscall overhead per run. That overhead is what the baseline row subtracts.

### Real ids start at 1, not 0

`balancer.bpf.c:150-155`:

```c
key = *real_pos;
if (key == 0) {
  // Real ids start from 1, so we don't map the id 0 to any real. This
  // is likely to happen if the ch ring for a vip is uninitialized.
  increment_ch_drop_real_0();
  return false;
}
```

The first version of this harness numbered backends from 0, which made `1/N` of
the ring dead and made the single-backend case drop 100% of packets. It showed
up as `XDP_DROP` on one row while the others reported plausible-looking
`XDP_TX` timings, so the corrupted rows were the ones that looked fine. The
harness now asserts `retval == XDP_TX` and the exact output length on every
row, and exits non-zero otherwise.

## Program and map footprint

Reported by libbpf at load time.

| | |
|---|---|
| program | `balancer_ingress`, section `xdp` |
| loaded instructions | **2,544** |
| object size | 132,816 bytes with BTF and debug info |
| maps created | 17 |

The two dominant maps, both `BPF_MAP_TYPE_ARRAY`, are allocated in full at load
time regardless of how many VIPs or backends are configured:

| map | entries | value | bytes |
|---|---:|---:|---:|
| `ch_rings` | 33,554,944 | 4 | 134,219,776 (**128.0 MiB**) |
| `server_id_map` | 16,777,214 | 4 | 67,108,856 (**64.0 MiB**) |

**192 MiB of BPF map memory before a single packet arrives**, in those two maps
alone. `ch_rings` is `MAX_VIPS * RING_SIZE` = 512 x 65537, the static product
recorded in `datapaths.md`; it is sized for the maximum VIP count whether or not
those VIPs exist. The remaining fifteen maps add a few MB, mostly per-CPU stats
arrays multiplied across 32 CPUs.

## Scope

Measured: katran. **Not measured: cilium.** Its BPF datapath needs generated
configuration headers that the Go agent emits at runtime (`node_config.h` and
the `CONFIG(...)` macros referenced throughout `bpf/lib/`), so it is not a
standalone clang invocation the way katran's is. That is a larger piece of work
and was not attempted here. dpvs remains DPDK-only and unmeasurable on this
host, per `target-host.md`.

So this is a single-implementation result, not yet a comparison.

## Reproducing

```bash
cd harness/run/bpf
make
sudo ./xdp_bench build/balancer.bpf.o 20000
```

Root is required for `bpf()` on this host. The build needs `clang`, `libbpf-dev`
and the kernel UAPI headers.

One non-obvious build flag, documented in the Makefile: `-D__x86_64__`. With
`-target bpf` clang does not define it, and glibc's `gnu/stubs.h` then selects
`gnu/stubs-32.h`, which is not installed on a 64-bit-only system. katran's BPF
source includes `<string.h>`, so it reaches that header.
