# l4lb-bench

Source material and inventories for a comparative performance study of canonical
L4 load balancers: katran, cilium's XDP path, and dpvs, against the papers the
designs come from.

**Phase 0 only. Nothing here builds or benchmarks anything.** This repo is
clones, papers, and notes about them. `harness/` is an empty scaffold.

## What you get

| path | what is in it |
|---|---|
| `notes/repos.md` | every cloned repo with commit SHA, branch, and clone date |
| `notes/build-requirements.md` | documented build sequence and declared dependencies per repo, transcribed from each project's own docs and CI, plus the DPDK version conflicts |
| `notes/datapaths.md` | per-packet fast path for katran, cilium, and dpvs, cited by `file:line` against the pinned SHAs: entry points, hash functions, lookup tables, conntrack, encap, maps, control-plane sync |
| `notes/tooling.md` | what measures cycles, PMU counters, flame graphs, eBPF program cost, DPDK internals, and per-packet latency, and whether it is installed |
| `env/host.md` | host capability report and a per-repo list of documented requirements this host does not meet |
| `env/host-raw.txt` | raw read-only probe output behind `host.md` |
| `env/clone.log` | raw clone output with SHAs |
| `papers/README.md` | all nine papers with source URL, size, and how each title was verified |
| `harness/` | empty scaffold, unimplemented |

## What you run

Nothing yet. To rehydrate the sources:

```bash
# repos/ is not committed. Every URL and SHA is in notes/repos.md.
mkdir -p repos && cd repos
git clone --depth 1 https://github.com/facebookincubator/katran
git clone --depth 1 https://github.com/cilium/cilium
git clone --depth 1 https://github.com/iqiyi/dpvs
# ...see notes/repos.md for all 15, and pin to the recorded SHAs
```

To pin one exactly as it was inventoried:

```bash
git -C repos/katran fetch --depth 1 origin e6f781e09144641967487f696a9a9f2e2975f4ef
git -C repos/katran checkout e6f781e09144641967487f696a9a9f2e2975f4ef
```

The papers are not committed either. `papers/README.md` has a direct URL for
each of the nine, all open access.

## What is not here, and why

- **`repos/`**: 1.1 GB of third-party source. Reproducible from `notes/repos.md`.
- **`papers/*.pdf`**: nine third-party PDFs. Reproducible from `papers/README.md`.
- **builds, logs, benchmarks**: out of scope for phase 0 by design.

## What you can change

Nothing here is parameterised, because nothing here runs. The two files that
drive later phases are `notes/repos.md`, which fixes the SHAs everything else
cites, and `env/host.md`, whose gap list says what a machine needs before any
of these projects will build.

## Host constraints found in phase 0

The machine this was inventoried on cannot currently build or run any of it:

- no C or C++ compiler, and no make, cmake, meson, or ninja
- zero hugepages reserved, so DPDK EAL will not initialise
- zero IOMMU groups and no vfio or uio loaded, so no NIC can bind to a userspace driver
- the only physical NIC is invisible inside WSL2; the guest sees a synthetic `hv_netvsc` device with MTU 1280
- AF_XDP zero-copy is unavailable on both relevant drivers, established from kernel source rather than by testing
- one NIC and one host, so no generator/DUT separation

Details and the per-repo gap list are in `env/host.md`.

## Notes on accuracy

Citations are `file:line` against the SHAs in `notes/repos.md`. Where something
could not be established from the sources, it is recorded as unknown rather than
inferred. Two known gaps: trex-core and beamer-doc keep their build instructions
on GitHub wikis that are not part of a git clone, and the Go writer for cilium's
LB maps was not traced (there is no `pkg/maps/lbmap` at the pinned SHA).
