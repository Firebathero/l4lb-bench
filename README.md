# l4lb-bench

Comparative performance study of canonical L4 load balancers: katran, cilium's
XDP path, and dpvs, measured against the papers the designs come from.

Nothing here reimplements an algorithm. Every measurement drives upstream code
compiled from pinned commits.

## What you run

```bash
cd harness/run && make && make run    # needs g++ and gcc only, ~15s
python3 harness/collect/pmu_probe.py  # check hardware counters work here
```

`repos/` and `papers/` are not committed. Both are reproducible:
`notes/repos.md` has every clone URL and SHA, `papers/README.md` has a direct
source URL for all nine papers.

## What you get

| path | what is in it |
|---|---|
| `notes/consistent-hashing.md` | **measured comparison** of Maglev, MaglevV2, AnchorHash and dpvs conhash: disruption, balance, update cost, cycles per lookup |
| `notes/xdp-datapath.md` | **per-packet cost of katran's real XDP datapath**, measured in-kernel with BPF_PROG_TEST_RUN, no NIC required |
| `notes/data/ch_bench.csv` | raw results, 55 rows |
| `notes/pmu-validation.md` | proof the hardware counters on this host are proportional and repeatable |
| `notes/target-host.md` | **what machine to buy next**: AF_XDP zero-copy support per NIC driver read from kernel source, and what each provisioning option unblocks |
| `notes/datapaths.md` | per-packet fast path for katran, cilium and dpvs, cited by `file:line` against the pinned SHAs |
| `notes/build-requirements.md` | documented build sequence and declared dependencies per repo, plus the DPDK version conflicts |
| `notes/repos.md` | every cloned repo with commit SHA, branch and clone date |
| `notes/tooling.md` | measurement tooling per category, present or absent |
| `env/host.md` | host capability report and per-repo list of unmet requirements |
| `harness/run/` | the benchmark driver plus `make verify`, 23 invariant checks |
| `harness/collect/` | PMU readers built on `perf_event_open(2)` |
| `papers/README.md` | nine papers, source URL and title verification |

## Findings so far

From `notes/consistent-hashing.md`, on real implementations at 1M keys:

- **AnchorHash and dpvs conhash are exactly minimally disruptive** on backend
  removal. To six decimal places, only the keys that were on the removed backend
  move. Maglev never is.
- **Maglev's excess disruption gets worse with smaller tables.** At 128 backends
  it breaks 1.75x more connections than necessary at katran's 65537-entry ring,
  and **4.6x** at dpvs's 4093-entry table.
- **conhash pays for consistency with imbalance and cost**: 26% peak-over-mean at
  128 backends against Maglev's 3%, and roughly 1000 cycles per lookup against
  Maglev's 8, because every lookup is an `snprintf`, an MD5 and an rbtree walk.
- **AnchorHash updates in constant time**, about 10,000x cheaper than
  regenerating a Maglev ring, which has no incremental update at all.
- Instruction counts are exact and repeat identically across runs; cycle counts
  carry up to 34% run-to-run spread on this host, so only large cycle gaps are
  claimed. `notes/consistent-hashing.md` records one claim withdrawn on that basis.
- **katran's XDP datapath costs ~300 ns per packet** net of a measured baseline, flat from 1 to 512 backends, and commits **192 MiB of BPF map memory at load time** before any traffic. See `notes/xdp-datapath.md`.
- **katran's `MaglevHash` silently ignores backend weights.** It applies them on
  the first pass then resets every weight to 1 (`MaglevHash.cpp:48-61`), so on a
  65537-entry ring the effect is swamped. `MaglevHashV2` honours them exactly.
  `CHFactory::make` defaults to the unweighted one.

## What is not measured yet

No packets. No throughput, no latency, no pps. The host this ran on cannot
produce those numbers: no compiler in the primary distro, zero hugepages, no
IOMMU groups, no NIC bindable to a userspace driver, and one NIC on one host so
no generator/DUT separation. Details and a per-repo gap list in `env/host.md`.

That work needs different hardware. dpvs in particular cannot be measured at all
on this machine, since it is DPDK-only, so the executable comparison is
currently katran against cilium plus algorithm-level results for everything else.

`notes/target-host.md` says what to provision. Short version: AWS ENA and Azure
MANA cannot do AF_XDP zero-copy, GCP gve and virtio_net can, and only bare metal
gives DPDK, dpvs, and a PMU with uncore counters.

## What you can change

- `harness/run/ch_bench.cpp`: backend counts, ring sizes, weighting, key count
- `notes/repos.md`: the SHAs every citation is made against

## Notes on accuracy

Citations are `file:line` against the SHAs in `notes/repos.md`. Where something
could not be established from the sources it is recorded as unknown rather than
inferred. Known gaps: cilium's Maglev is Go and was not built, so only its table
size (16381) carried across; dpvs's own `mh` scheduler does not separate from
DPDK, so only its table size (4093) carried across; trex-core and beamer-doc
keep build instructions on GitHub wikis that are not part of a git clone.

Two build deviations are disclosed in `harness/run/README.md`: a DPDK allocator
shim needed to compile dpvs's libconhash outside DPDK, and a no-op node
finaliser the harness must pass to `conhash_fini`. No upstream file is patched.
