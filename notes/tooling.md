# Tooling survey

What exists per category, whether it is on this host, the install command if it
is not, and one line on what it measures. Nothing here was installed or run.
No recommendations and no ranking.

Package versions are the apt **candidate** versions from the configured Ubuntu
24.04 sources as queried with `apt-cache policy`, not installed versions.

---

## 1. Cycle and instruction counting at function granularity

| tool | present | install | what it measures |
|---|---|---|---|
| `perf stat` / `perf record` / `perf annotate` | **no** | `apt install linux-tools-common linux-tools-generic` (candidate **6.8.0-124.124**) | hardware and software event counts, sampled to symbol and instruction level |
| `perf` matching this kernel | **no, and not packaged** | `apt-cache policy linux-tools-6.18.33.2-microsoft-standard-WSL2` returns nothing. The archive only carries `linux-tools` for Ubuntu's own 6.8.0 kernels. Building from the WSL2 kernel tree is the other documented route. | as above |
| `valgrind --tool=callgrind` | **no** | `apt install valgrind` (candidate **1:3.22.0-0ubuntu3**) | simulated instruction counts and call graph, no real PMU, serialised execution |
| `gprof` | **no** (binutils absent) | `apt install binutils` | sampled flat profile plus call graph from compiler `-pg` instrumentation |
| `hotspot` | **no** | `apt install hotspot` (candidate **1.3.0-2ubuntu4**) | GUI front end over `perf.data`, per-symbol and per-line cost |
| `ftrace` / `trace-cmd` `function_graph` | **no** (`trace-cmd`) | `apt install trace-cmd` (candidate **3.2-1ubuntu2**) | per-function entry and exit with duration, kernel side |
| `rdtsc` / `rdtscp` in-code | n/a | none | cycle timestamps read directly by the program under test |

Host notes: `rdtscp`, `constant_tsc`, `nonstop_tsc`, and `tsc_reliable` are all
present, so an in-code TSC read is available without a package. Absence of a
compiler blocks anything that requires rebuilding the target with
instrumentation.

---

## 2. PMU access

### Counters this CPU exposes

Read from `/sys/bus/event_source/devices/cpu/events/` on this host. The CPU is
an AMD Ryzen 9 7950X3D (Zen 4, family 25 model 97) under Hyper-V.

| exported alias | category |
|---|---|
| `cpu-cycles` | cycles |
| `ref-cycles` | cycles |
| `instructions` | retire |
| `cache-references` | cache |
| `cache-misses` | cache |
| `branch-instructions` | branch |
| `branch-misses` | branch |
| `stalled-cycles-frontend` | stalls |

Event sources present: `breakpoint`, `cpu` (type 4), `kprobe`, `msr`,
`software`, `tracepoint`, `uprobe`. The `msr` PMU exports one event, `tsc`.

**Not exported here:**

- no TLB events (`dTLB-*`, `iTLB-*` aliases absent from `cpu/events/`)
- no `stalled-cycles-backend`
- `/sys/bus/event_source/devices/cpu/caps/max_precise = 0`, so no precise
  sampling. On AMD that means no IBS backing for `:p` / `:pp` modifiers.
- no uncore, L3, or data-fabric PMUs (`amd_df`, `amd_l3`, `amd_umc` are absent
  from the event source list)

`perf_event_paranoid = 2` and `kptr_restrict = 1` gate unprivileged kernel-level
measurement.

**Unresolved, flagged:** the aliases above are what the kernel driver declares.
Whether Hyper-V actually virtualises the underlying counters through to this
guest cannot be determined from sysfs, and confirming it requires executing a
counter read, which this phase does not do. Treat the table as declared, not
validated.

### pmu-tools / toplev

| tool | present | install | what it measures |
|---|---|---|---|
| `pmu-tools` (`toplev`, `ocperf`) | **no** | `git clone https://github.com/andikleen/pmu-tools` (no apt package) | Top-down Microarchitecture Analysis: attributes cycles to frontend bound, backend bound, bad speculation, retiring |

Applicability to this host: `toplev` implements Intel's Top-down
Microarchitecture Analysis method and ships Intel event files. This CPU is AMD
Zen 4. The vendor equivalent for AMD is AMD uProf, distributed by AMD rather
than through the Ubuntu archive. Separately, the TMA breakdown needs
frontend and backend stall events plus uncore access; this host exports
`stalled-cycles-frontend` only, no backend-stall alias and no uncore PMU.

---

## 3. Flame graph generation

| tool | present | install | what it measures |
|---|---|---|---|
| FlameGraph scripts (`stackcollapse-perf.pl`, `flamegraph.pl`) | **no** | `git clone https://github.com/brendangregg/FlameGraph` (no apt package) | folds sampled stacks into an SVG showing time share per stack frame |
| `perf script` (input side) | **no** | with `linux-tools-generic` above | dumps raw samples with call chains for folding |
| `hotspot` | **no** | `apt install hotspot` (candidate **1.3.0-2ubuntu4**) | renders flame graphs directly from `perf.data` |
| `bpftrace` stack aggregation | **no** | `apt install bpftrace` (candidate **0.20.2-1ubuntu4.3**) | aggregates `kstack` / `ustack` counts, foldable into a flame graph |
| `profile` from bcc | **no** | `apt install bpfcc-tools` (candidate **0.29.1+ds-1ubuntu7**) | timed stack sampling via `perf_event`, prints folded stacks |

All of these depend on working sampling, which depends on the PMU question in
section 2 and on `perf_event_paranoid`.

---

## 4. eBPF program profiling

| tool | present | install | what it measures |
|---|---|---|---|
| `bpftool prog profile` | **no** | **no `bpftool` apt candidate on this host** (`apt-cache policy bpftool` returns `Candidate: (none)`). It ships inside `linux-tools-common` / `linux-tools-<ver>`, or builds from `kernel/tools/bpf/bpftool`. | per-BPF-program counters over the program's own execution window |
| `bpftool prog show` / `dump xlated` / `dump jited` | **no** | as above | program metadata, verified instruction count, JIT-compiled output |
| `bpftool map dump` | **no** | as above | map contents, so LB table state can be read out |
| `bpftrace` | **no** | `apt install bpftrace` | attaches to kprobes, uprobes, tracepoints and aggregates; can instrument the kernel around the BPF program |
| bcc (`python3-bpfcc`, `bpfcc-tools`) | **no** | `apt install bpfcc-tools python3-bpfcc` | same class of instrumentation from Python |
| `pahole` (`dwarves`) | **no** | `apt install dwarves` (candidate **1.25-0ubuntu3**) | struct layout and BTF generation, used for CO-RE and for checking map value layout |
| clang / LLVM for BPF | **no** | `apt install clang llvm` (candidate **1:18.0-59~exp2**, so LLVM **18**) | compiles the BPF objects |

`bpftool prog profile` scope, stated plainly:

- **can see**: `cycles`, `instructions`, `l1d_loads`, `llc_misses`,
  `itlb_misses`, `dtlb_misses` accumulated across runs of one BPF program,
  by enabling counters on entry and disabling them on exit. It gives a
  per-program total and a run count.
- **cannot see**: anything inside the program. There is no per-instruction, per
  line, or per-helper attribution. It does not separate tail-call targets from
  the caller beyond program boundaries, it does not attribute cost to individual
  map lookups, and it reports no distribution, only totals from which a mean is
  derived.
- **depends on**: working `perf_event` hardware counters, which is the open
  question in section 2. On this host the subset it wants (`l1d_loads`,
  `llc_misses`, `itlb_misses`, `dtlb_misses`) maps to events that are **not**
  in the exported alias list.

Note for cilium specifically: its builder image pins LLVM **19.1.7**
(`images/builder/Dockerfile:9`) while the Ubuntu 24.04 archive candidate is
LLVM **18**. Recorded as a version gap, not resolved.

### In-repo instrumentation already present

| project | files | what it produces |
|---|---|---|
| katran | `katran/lib/bpf/flow_debug.h`, `flow_debug_helpers.h`, `flow_debug_maps.h`, `introspection.h` | per-packet flow debug records emitted from the XDP program; consumed by `KatranMonitor` / `KatranEventReader` on the C++ side |
| cilium | `bpf/lib/events.h`, `notify.h`, `dbg.h`, `metrics.h` | per-packet trace and drop notifications over a perf ring buffer, plus aggregate metrics maps |
| dpvs | `src/pdump.c`, `src/iftraf.c` | packet capture hooks and per-interface traffic accounting inside the DPDK process |
| trex-core | `src/hdrh/`, `src/flow_stat_parser.{cpp,h}`, `src/stateful_rx_core.{cpp,h}` | HDR histogram library vendored in-tree, plus per-flow statistics parsing on the rx path |

---

## 5. DPDK-side instrumentation

Everything below is present **in the DPDK source clone** at
`repos/dpdk` (26.11.0-rc0). None of it is built or installed, because DPDK is
not built.

| component | location in clone | what it measures |
|---|---|---|
| `rte_telemetry` | `lib/telemetry/` | in-process telemetry socket; libraries and PMDs register callbacks returning counters as JSON |
| `dpdk-telemetry.py` | `usertools/dpdk-telemetry.py` | interactive client for the telemetry socket |
| `dpdk-telemetry-client.py` | `usertools/` | scripted client |
| `dpdk-telemetry-exporter.py` | `usertools/` | exports telemetry for external scrapers |
| `dpdk-telemetry-watcher.py` | `usertools/` | polls telemetry endpoints over time |
| telemetry endpoint definitions | `usertools/telemetry-endpoints/` | the endpoint set the watcher and exporter use |
| `rte_pdump` | `lib/pdump/` | packet capture framework inside a running DPDK process |
| `dpdk-pdump` | `app/pdump/` | secondary-process client that pulls captured packets out to a pcap |
| `dpdk-dumpcap` | `app/dumpcap/` | Wireshark-style capture front end over pdump |
| `dpdk-proc-info` | `app/proc-info/` | port, queue, xstats, and mempool dumps from a secondary process |
| `rte_latencystats` | `lib/latencystats/` | per-packet latency, average, jitter, and max, computed from mbuf timestamps |
| `rte_metrics` | `lib/metrics/` | generic named-metric registry that latencystats and bitratestats report through |
| `rte_bitratestats` | `lib/bitratestats/` | peak and average bit rate per port |
| `rte_bpf` | `lib/bpf/` | in-DPDK BPF interpreter and JIT, used by pdump for capture filters |
| `dpdk-testpmd` | `app/test-pmd/` | forwarding test application with per-port xstats |
| `dpdk-devbind.py` | `usertools/dpdk-devbind.py` | lists and binds NIC drivers (setup, not measurement) |
| `dpdk-hugepages.py` | `usertools/dpdk-hugepages.py` | reserves and reports hugepages (setup, not measurement) |
| `cpu_layout.py` | `usertools/cpu_layout.py` | prints socket, core, and thread layout for pinning decisions |
| `dpdk-rss-flows.py` | `usertools/dpdk-rss-flows.py` | computes which RSS queue a given flow lands on |

DPVS extends `dpdk-pdump` with packet filters via a patch shipped in the repo,
`patch/dpdk-24.11/0001-pdump-add-cmdline-packet-filters-for-dpdk-pdump-tool.patch`,
documented at `dpvs/doc/tutorial.md:1484`.

---

## 6. Per-packet latency histogram capture

| tool | present | install | what it measures |
|---|---|---|---|
| `rte_latencystats` | in DPDK clone, not built | build DPDK | per-packet latency from mbuf timestamp to tx, reported as min, avg, max, jitter through `rte_metrics` |
| TRex HDR histogram | in trex-core clone at `src/hdrh/`, not built | build trex-core | full latency distribution at high dynamic range, per stream |
| `bpftrace` `hist()` / `lhist()` | **no** | `apt install bpftrace` | log2 or linear histogram of any traced value, including timestamp deltas |
| bcc `funclatency`, `biolatency` pattern | **no** | `apt install bpfcc-tools` | in-kernel histogram of function or event latency without per-event userspace cost |
| `perf` with `SO_TIMESTAMPING` | **no** | `linux-tools-generic` | NIC or kernel timestamps per packet; hardware timestamping requires NIC support, and `hv_netvsc` is a synthetic device |
| `sockperf`, `netperf` | **no** | `apt install sockperf` / `netperf` | request-response latency percentiles at the socket layer, not per packet in the datapath |

---

## 7. Anything producing per-packet rather than aggregate numbers

| tool | present | install | granularity produced |
|---|---|---|---|
| `dpdk-pdump` / `rte_pdump` | in clone, not built | build DPDK | every captured packet, written to pcap |
| `dpdk-dumpcap` | in clone, not built | build DPDK | every captured packet, Wireshark-compatible |
| katran flow debug maps | in clone, not built | build katran | one record per packet from inside the XDP program, over a perf ring buffer |
| cilium monitor events (`events.h`, `notify.h`) | in clone, not built | build cilium | one trace or drop notification per packet, over a perf ring buffer |
| dpvs `src/pdump.c` | in clone, not built | build dpvs | packet capture inside the DPDK process |
| `bpftrace` with `printf` per event | **no** | `apt install bpftrace` | one line per traced event; aggregation is optional |
| bcc `trace` | **no** | `apt install bpfcc-tools` | one line per matched event |
| `tcpdump` / AF_PACKET | **no** | `apt install tcpdump` | every packet the kernel path sees; does not see XDP-dropped or XDP-redirected packets |
| `perf record` with tracepoints | **no** | `linux-tools-generic` | one sample per event rather than a counter total |
| `trace-cmd` / ftrace | **no** | `apt install trace-cmd` | one record per kernel function entry and exit |
| `xdpdump` | **no** | not in the Ubuntu 24.04 archive; ships with `xdp-tools` | captures packets at the XDP hook, before and after the XDP program runs |

Structural note relevant to this category: `tcpdump` and any AF_PACKET-based
capture sit above XDP in the receive path, so packets an XDP program returns
`XDP_DROP` or `XDP_TX` for never reach them. `xdpdump` and the projects' own
in-program ring buffers are the tools that see the XDP-handled packets.

---

## Supporting utilities absent on this host

| tool | install | purpose |
|---|---|---|
| `numactl` | `apt install numactl` (candidate **2.0.18-1ubuntu0.24.04.1**) | NUMA node binding and reporting; this host reports a single node |
| `ethtool` | `apt install ethtool` (candidate **1:6.7-1build1**) | queue counts, driver info, per-queue stats, XDP feature flags |
| `lspci` (`pciutils`) | `apt install pciutils` (candidate **1:3.10.0-2build1**) | PCI device and driver enumeration for NIC binding |
| `libbpf-dev` | `apt install libbpf-dev` (candidate **1:1.3.0-2build2**) | headers for building against libbpf; only the runtime `libbpf.so.1` is present |
