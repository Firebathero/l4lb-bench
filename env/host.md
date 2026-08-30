# Host capability report

Collected 2026-08-30T12:24:47Z by read-only inspection. Nothing was installed,
loaded, mounted, or reconfigured.

## Where this report was taken

**Important framing fact.** The deliverable root `~/l4lb-bench` resolves to
`/home/<user>/l4lb-bench` inside **WSL2, distro `Ubuntu-24.04`**, running on a
Windows 11 host. The task's Linux-only checks (`lscpu`, `/sys/kernel/btf`,
hugepages, IOMMU, `vfio-pci`, `perf_event_paranoid`) only exist in a Linux
kernel, and WSL2 was the only Linux on this machine. Two other distros are
registered: `Ubuntu-22.04` (works, runs as root) and `Ubuntu` (**broken**, its
`ext4.vhdx` is missing, `Wsl/Service/CreateInstance/MountDisk/HCS/ERROR_FILE_NOT_FOUND`).

WSL2 runs a Microsoft-built kernel in a Hyper-V utility VM with synthetic
devices. Where the virtualised view differs from the physical machine, both are
given below.

## CPU

Identical silicon reported from both sides.

| property | value |
|---|---|
| model | AMD Ryzen 9 7950X3D 16-Core Processor |
| family / model / stepping | 25 / 97 / 2 (Zen 4) |
| sockets / cores / threads | 1 / 16 / 32 (2 threads per core) |
| max clock (Windows) | 4201 MHz |
| BogoMIPS (WSL2) | 8384.03 |
| virtualisation | AMD-V; hypervisor vendor Microsoft; type full |
| motherboard | Gigabyte B650 AORUS ELITE AX V2, socket AM5 |

Topology from `lscpu -p`: CPUs 0-31, cores 0-15, socket 0, node 0. Sibling pairs
are adjacent (`0,1` share core 0; `2,3` share core 1; and so on).

### Cache

Per-CPU sysfs (`/sys/devices/system/cpu/cpu0/cache`), as seen inside WSL2:

| level | type | size | ways | line | shared by |
|---|---|---|---|---|---|
| L1d | Data | 32 KiB | 8 | 64 B | 0-1 |
| L1i | Instruction | 32 KiB | 8 | 64 B | 0-1 |
| L2 | Unified | 1024 KiB | 8 | 64 B | 0-1 |
| L3 | Unified | 98304 KiB (96 MiB) | 16 | 64 B | **0-31** |

`lscpu` aggregate: L1d 512 KiB (16 instances), L1i 512 KiB (16), L2 16 MiB (16),
L3 96 MiB (**1 instance**).

**Discrepancy, flagged not resolved.** Windows `Win32_Processor` reports
`L3CacheSize = 131072` KB (128 MiB). WSL2 sysfs reports a single 96 MiB L3
instance shared by all 32 logical CPUs. The two numbers disagree, and the WSL2
view presents one flat L3 domain rather than any per-CCD split. Any measurement
that depends on last-level-cache residency or on cache-domain-aware core pinning
will be reading this flattened topology, not whatever the physical part does.

### Flags relevant to packet work

Present: `sse4_2` (CRC32), `avx`, `avx2`, `avx512f`, `avx512dq`, `avx512bw`,
`avx512vl`, `avx512cd`, `avx512ifma`, `avx512vbmi`, `avx512_vbmi2`,
`avx512_vnni`, `avx512_bitalg`, `avx512_vpopcntdq`, `avx512_bf16`, `aes`,
`vaes`, `pclmulqdq`, `vpclmulqdq`, `sha_ni`, `gfni`, `rdtscp`, `constant_tsc`,
`nonstop_tsc`, `tsc_reliable`, `tsc_known_freq`, `pdpe1gb` (1 GiB pages),
`clflushopt`, `clwb`, `movbe`, `bmi1`, `bmi2`, `erms`, `invpcid`, `popcnt`,
`rdrand`, `rdseed`, `rdpid`, `fsrm`, `umip`, `hypervisor`.

Absent: `x2apic`, `tsc_deadline_timer`, `movdir64b`.

Note on invariant TSC: Linux does not print a flag literally named
`invariant_tsc`; the equivalent is `nonstop_tsc`, which **is** present, alongside
`constant_tsc` and `tsc_reliable`.

## Memory and NUMA

| property | WSL2 view | Windows host |
|---|---|---|
| total RAM | 15912664 kB (15.2 GiB) | 31.11 GiB |
| free at collection | 13487076 kB | n/a |
| NUMA nodes | 1 (`node0`, CPUs 0-31) | 1 processor package |

WSL2 was allocated roughly half the physical RAM, which is the default.

## Kernel

```
Linux <host> 6.18.33.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC
Thu Jun 18 21:54:43 UTC 2026 x86_64
```

Built with GCC 13.2.0 / GNU ld 2.41. Distro userland: Ubuntu 24.04.1 LTS.

Kernel cmdline:
`initrd=\initrd.img WSL_ROOT_INIT=1 panic=-1 nr_cpus=32 hv_utils.timesync_implicit=1 console=hvc0 debug pty.legacy_count=0 WSL_ENABLE_CRASH_DUMP=1`

### BTF

`/sys/kernel/btf/vmlinux` is **present**, 6677359 bytes. `CONFIG_DEBUG_INFO_BTF=y`
and `CONFIG_DEBUG_INFO_BTF_MODULES=y`. Per-module BTF is exposed for the loaded
set (`bridge`, `br_netfilter`, `ip_tables`, `kvm`, `kvm_amd`, `tun`, and others).

### Config, read from `/proc/config.gz`

`/boot/config-$(uname -r)` is absent; `/proc/config.gz` is present and was used.

BPF and XDP:
```
CONFIG_BPF=y                      CONFIG_BPF_JIT=y
CONFIG_BPF_SYSCALL=y              CONFIG_BPF_JIT_ALWAYS_ON=y
CONFIG_BPF_EVENTS=y               CONFIG_BPF_JIT_DEFAULT_ON=y
CONFIG_BPF_LSM=y                  CONFIG_HAVE_EBPF_JIT=y
CONFIG_CGROUP_BPF=y               CONFIG_BPF_UNPRIV_DEFAULT_OFF=y
CONFIG_XDP_SOCKETS=y              CONFIG_XDP_SOCKETS_DIAG=m
CONFIG_NET_CLS_BPF=m              CONFIG_NET_SCH_INGRESS=m
CONFIG_NET_ACT_BPF=m              CONFIG_NET_CLS_ACT=y
```

IPVS is available as modules, which is relevant since IPVS is itself an L4 load
balancer:
```
CONFIG_IP_VS=m           CONFIG_IP_VS_MH=m        CONFIG_IP_VS_SH=m
CONFIG_IP_VS_RR=m        CONFIG_IP_VS_WRR=m       CONFIG_IP_VS_DH=m
CONFIG_IP_VS_LC=m        CONFIG_IP_VS_WLC=m       CONFIG_IP_VS_SED=m
CONFIG_IP_VS_NQ=m        CONFIG_IP_VS_LBLC=m      CONFIG_IP_VS_LBLCR=m
CONFIG_IP_VS_TAB_BITS=12         CONFIG_IP_VS_MH_TAB_INDEX=12
CONFIG_IP_VS_SH_TAB_BITS=8       CONFIG_IP_VS_NFCT=y
CONFIG_IP_VS_PROTO_TCP/UDP/SCTP/AH/ESP=y          CONFIG_IP_VS_DEBUG=y
```

Other: `CONFIG_VXLAN=y`, `CONFIG_GENEVE=m`, `CONFIG_FIB_RULES=y`,
`CONFIG_CRYPTO_SHA1=y`, `CONFIG_CRYPTO_USER_API_HASH=m`, `CONFIG_PERF_EVENTS=y`,
`CONFIG_SCHEDSTATS=y`, `CONFIG_CGROUP_NET_CLASSID=y`, `CONFIG_HUGETLBFS=y`,
`CONFIG_HUGETLB_PAGE=y`, `CONFIG_KPROBES=y`, `CONFIG_UPROBES=y`,
`CONFIG_KALLSYMS=y`, `CONFIG_FUNCTION_TRACER=y`, `CONFIG_DEBUG_INFO=y`,
`CONFIG_IOMMU_SUPPORT=y`, `CONFIG_AMD_IOMMU=y`, `CONFIG_INTEL_IOMMU=y`,
`CONFIG_VFIO=m`, `CONFIG_VFIO_PCI=m`, `CONFIG_VFIO_IOMMU_TYPE1=m`,
`CONFIG_UIO=m`.

### BPF runtime state

```
/proc/sys/kernel/unprivileged_bpf_disabled = 2
/proc/sys/net/core/bpf_jit_enable          = 1
/proc/sys/net/core/bpf_jit_harden          = (not present)
/sys/fs/bpf                                = present (bpffs mounted)
```

`unprivileged_bpf_disabled = 2` means `bpf()` is refused for unprivileged
callers; loading programs requires root or `CAP_BPF`.

## NIC inventory

### Inside WSL2

| iface | driver | MTU | state | queues | speed | address |
|---|---|---|---|---|---|---|
| `eth0` | `hv_netvsc` | **1280** | up | 16 rx / 16 tx | 10000 | <wsl-ipv4-redacted> |
| `lo` | none | 65536 | unknown | 1 / 1 | n/a | 127.0.0.1/8, 10.255.255.254/32 |

`/sys/bus/pci/devices` contains 4 entries, all synthetic Hyper-V VMBus IDs
(`31eb:00:00.0`, `5582:00:00.0`, `9c57:00:00.0`, `a5c1:00:00.0`). No physical
NIC is passed through. `eth0` exposes no `xdp_features` sysfs attribute; that
field is read over netlink or `ethtool`, neither of which is available here
(`ethtool` is not installed and this phase installs nothing).

### Physical, from the Windows side

| adapter | driver | link | PCI ID |
|---|---|---|---|
| Realtek Gaming 2.5GbE Family Controller | `rt25cx21x64.sys` | **1 Gbps** | `PCI\VEN_10EC&DEV_8125` |

That is the **only** physical Ethernet NIC. Everything else Windows reports is
virtual: Hyper-V vSwitch/vEthernet for WSL, a VirtualBox host-only adapter, a
Tailscale `wintun` tunnel, Bluetooth PAN, and WAN miniports. `RTL8125` is a
2.5GbE part currently negotiated at 1 Gbps.

### XDP and AF_XDP support for these drivers

Determined by reading kernel source and `Documentation/networking/af_xdp.rst`
from `torvalds/linux` master, not by testing.

**`hv_netvsc`** (`drivers/net/hyperv/netvsc_drv.c`):

- `.ndo_bpf = netvsc_bpf` at line 1992, so native XDP program attach is supported
- `.ndo_xdp_xmit = netvsc_ndoxdp_xmit` at line 1993, so `XDP_REDIRECT` egress works
- **no `.ndo_xsk_wakeup`**, so **no AF_XDP zero-copy**
- declared features at lines 2583-2584:
  `NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT | NETDEV_XDP_ACT_NDO_XMIT`.
  `NETDEV_XDP_ACT_XSK_ZEROCOPY` is not among them.
- the receive path handles only `XDP_PASS` and `XDP_TX` as non-error actions
  (line 861), with `XDP_REDIRECT` handled separately (line 858)

**`r8169`** (`drivers/net/ethernet/realtek/r8169_main.c`), the Linux driver for
the physical RTL8125:

- zero occurrences of `xdp` or `xsk` anywhere in the file
- no `ndo_bpf`, no `ndo_xdp_xmit`, no `ndo_xsk_wakeup`
- **no native XDP and no AF_XDP zero-copy.** XDP would run in generic/SKB mode only.

Per `af_xdp.rst` lines 248-255, an `XDP_ZEROCOPY` bind fails outright when the
driver does not support it, and the default bind silently falls back to copy
mode. Lines 738-741 name `NETDEV_XDP_ACT_XSK_ZEROCOPY` as the discovery flag.

## Hugepages

| item | value |
|---|---|
| `Hugepagesize` | 2048 kB |
| `HugePages_Total` / `_Free` / `_Rsvd` / `_Surp` | 0 / 0 / 0 / 0 |
| `hugepages-2048kB/nr_hugepages` | 0 |
| `hugepages-1048576kB/nr_hugepages` | 0 (1 GiB pages supported, `pdpe1gb` present) |
| hugetlbfs mount | `hugetlbfs /dev/hugepages hugetlbfs rw,nosuid,nodev,relatime,pagesize=2M` |
| `AnonHugePages` | 0 kB |
| transparent hugepages | `always [madvise] never` |

hugetlbfs is mounted but **no hugepages are reserved**.

## IOMMU and vfio-pci

| item | state |
|---|---|
| `/sys/kernel/iommu_groups` | **0 groups** |
| `/sys/class/iommu` | exists, empty |
| `iommu=` / `intel_iommu=` / `amd_iommu=` in cmdline | **absent** |
| `vfio`, `vfio-pci`, `vfio_iommu_type1` modules loaded | **no** |
| `vfio*.ko` present on disk | yes, under `/lib/modules/6.18.33.2-microsoft-standard-WSL2/kernel/drivers/vfio/` (`vfio.ko`, `vfio-pci.ko`, `vfio-pci-core.ko`, `vfio_iommu_type1.ko`) |
| `uio` module loaded | no |

The modules are shipped but no IOMMU is exposed to the guest, so there are no
groups to bind a device into.

## perf and PMU

| item | value |
|---|---|
| `/proc/sys/kernel/perf_event_paranoid` | **2** |
| `/proc/sys/kernel/kptr_restrict` | 1 |
| `/proc/sys/kernel/nmi_watchdog` | 0 |
| `perf` binary | **not installed** |

Event sources under `/sys/bus/event_source/devices/`: `breakpoint`, `cpu`,
`kprobe`, `msr`, `software`, `tracepoint`, `uprobe`.

The `cpu` PMU (`type = 4`) exports these hardware event aliases:

```
branch-instructions  branch-misses  cache-misses  cache-references
cpu-cycles           instructions   ref-cycles    stalled-cycles-frontend
```

`/sys/bus/event_source/devices/cpu/caps/max_precise = 0`. Precise-event sampling
(AMD IBS backing for `:p` modifiers) is **not** available. `stalled-cycles-backend`
is not exported. The `msr` PMU exports only `tsc`.

`perf_event_paranoid = 2` blocks unprivileged kernel-level measurement;
user-space sampling is allowed. Whether the Hyper-V hypervisor actually
virtualises these counters through to the guest cannot be established from
sysfs alone, and confirming it would mean executing a counter read, which this
phase does not do. Treat the aliases above as *declared*, not *validated*.

## Tool presence

Present:

| tool | version |
|---|---|
| `git` | 2.43.0 |
| `curl` | 8.5.0 |
| `python3` | 3.12.3 |
| `wget`, `lscpu`, `lshw`, `file`, `tar`, `xz`, `dpkg-query`, `awk`, `sed`, `grep` | (base system) |
| `ip`, `tc` | iproute2, present |
| `libbpf.so.1` | `/lib/x86_64-linux-gnu/libbpf.so.1`, package `libbpf1 1:1.3.0-2build2` (runtime only, no headers) |
| `libelf1t64` | 0.190-1.1ubuntu0.1 |
| `gcc-14-base` | 14.2.0-4ubuntu2~24.04.1 (metapackage only, no compiler binary) |

**Absent** (not installed, per the task's instruction to list and not install):

`gcc`, `g++`, `make`, `cmake`, `meson`, `ninja`, `pkg-config`, `clang` (any
version), `llvm-config`, `ld.lld`, `bpftool`, `pahole` / `dwarves`, `perf`,
`bcc` / `python3-bpfcc`, `bpftrace`, `go`, `rustc`, `ethtool`, `lspci`,
`numactl`, `nasm`, `unzip`, `libbpf-dev`, `libnuma-dev`, `libpcap-dev`,
`libbsd-dev`, `python3-pyelftools`, `linux-headers-*`, `linux-tools-*`.

`pkg-config --modversion libdpdk` fails: no DPDK installed.

This host has **no C or C++ compiler at all**.

## Generator / DUT separation

**Not available. This is a hard constraint.**

- one physical NIC (Realtek RTL8125, linked at 1 Gbps)
- one usable interface inside WSL2 (`eth0`, synthetic `hv_netvsc`, MTU 1280)
- no second physical NIC, no second host, no NIC that can be bound to a
  userspace driver

Generator and device-under-test would have to share the same host and, inside
WSL2, the same synthetic interface. Note also that `eth0` has **MTU 1280**,
below the 1500 that most of these projects assume.

## Gap list

Documented requirement versus current state. Facts only; closing these is out of
scope for this phase.

### katran

| requirement | source | host state |
|---|---|---|
| Linux kernel 5.6+ | README.md:40 | **met** (6.18.33.2) |
| clang 6.0+ | README.md:41 | **absent** |
| build-essential / C++17 toolchain | README.md:42, CMakeLists.txt:22 | **absent** (no gcc/g++) |
| cmake >= 3.9 | CMakeLists.txt:1 | **absent** |
| folly, fizz, fmt, glog, gflags, googletest, boost, libevent, libsodium, libunwind, double-conversion, fast_float, lz4, snappy, zstd, openssl, libdwarf, libaio, libmnl, libiberty, liboqs, xz, zlib | getdeps_linux.yml fetch steps; CMakeLists.txt:30-36 | **all absent** |
| libbpf + libelf | getdeps_linux.yml; build_katran.sh:117 (`libbpfcc-dev`) | libbpf runtime only, **no headers**; libelf runtime only |
| Linux kernel source tree at `deps/linux/` | DEVELOPING.md | **absent** |
| ninja | getdeps_linux.yml | **absent** |
| XDP driver mode for full speed | README.md:17,133-137 | `hv_netvsc` supports native XDP; the physical r8169 does not |
| root for BPF test runner | DEVELOPING.md (`os_run_tester.sh`) | available, but `unprivileged_bpf_disabled=2` |

### cilium

| requirement | source | host state |
|---|---|---|
| Go 1.26.0 | go.mod:3 | **absent** |
| clang (BPF datapath is clang-only) | images/builder/Dockerfile:87-90 | **absent** |
| LLVM 19.1.7 image `cilium/cilium-llvm:19.1.7-...` | images/builder/Dockerfile:9 | **absent** |
| `llvm-objcopy`, `llvm-strip` | images/builder/Dockerfile:90 | **absent** |
| Linux kernel >= 5.10 | Documentation/operations/system_requirements.rst:23,40 | **met** (6.18.33.2) |
| `CONFIG_BPF`, `BPF_EVENTS`, `BPF_SYSCALL`, `BPF_JIT`, `CGROUP_BPF`, `DEBUG_INFO_BTF`, `PERF_EVENTS`, `SCHEDSTATS`, `CGROUPS`, `CRYPTO_SHA1`, `CRYPTO_USER_API_HASH` | system_requirements.rst:157-170 | **all met** |
| `CONFIG_NET_CLS_BPF`, `NET_CLS_ACT`, `NET_SCH_INGRESS` | system_requirements.rst:160-163 | **met** (as modules) |
| `CONFIG_VXLAN`, `CONFIG_GENEVE`, `CONFIG_FIB_RULES` | system_requirements.rst:195-197 | **met** |
| Docker/BuildKit for the builder image | images/builder/Dockerfile | **absent** (no docker on this host) |
| Ubuntu 26.04 base image | images/builder/Dockerfile:6 | n/a, distro is 24.04 |

### dpvs

| requirement | source | host state |
|---|---|---|
| **DPDK 24.11** | README.md:81,85; `.github/workflows/build.yaml:27` | cloned DPDK is **26.11.0-rc0**. See conflict note below. |
| DPDK patches from `patch/dpdk-24.11/` | README.md:105-110 | present in repo, not applied |
| meson + ninja | README.md:123-125 | **both absent** |
| gcc (verified with 8.5) | README.md:65 | **absent** |
| `PKG_CONFIG_PATH` to `libdpdk.pc`; `pkg-config` | README.md:126,184; src/dpdk.mk | `pkg-config` **absent**, no libdpdk |
| hugepages, 8192 x 2 MiB per NUMA node | README.md:135-136 | **0 reserved** (hugetlbfs is mounted) |
| `uio_pci_generic` / `vfio-pci` / `igb_uio` bound to the NIC | README.md:158,164-169 | **no IOMMU groups, no vfio loaded, no bindable physical NIC** |
| `lspci` / `ethtool` to find PCI bus id | README.md:173 | **both absent** |
| `rte_flow` with ipv4/ipv6/tcp/udp items and drop/queue actions for multi-core FNAT/SNAT | README.md:62 | no DPDK-capable NIC present |
| verified NICs: Intel IXGBE, NVIDIA MLX5 | README.md:67 | neither present |
| root | README.md (devbind, modprobe) | available |

### DPDK

| requirement | source | host state |
|---|---|---|
| C compiler with C11 + standard atomics, GCC 8.0+ or Clang 7+ | doc/guides/linux_gsg/sys_reqs.rst | **absent** |
| `pkg-config` or `pkgconf` | sys_reqs.rst | **absent** |
| Python 3.6+ | sys_reqs.rst | **met** (3.12.3) |
| Meson 0.57+ and ninja | sys_reqs.rst; meson.build:13 (`meson_version: '>= 0.57.2'`) | **both absent** |
| `pyelftools` 0.22+ | sys_reqs.rst | **absent** |
| `libnuma-dev` | sys_reqs.rst | **absent** |
| `libelf` (to build the bpf library) | sys_reqs.rst | runtime only, no headers |
| CI build deps: `libarchive-dev libbsd-dev libbpf-dev libfdt-dev libibverbs-dev libipsec-mb-dev libisal-dev libjansson-dev libnuma-dev libpcap-dev libssl-dev libvirt-dev ninja-build pkg-config python3-pyelftools zlib1g-dev` | `.github/workflows/build.yml` `build_deps` | **all absent** |
| hugepages for runtime | linux_gsg | **0 reserved** |

### pktgen-dpdk

| requirement | source | host state |
|---|---|---|
| "the latest DPDK", no pinned version | INSTALL.md:59-61 | cloned DPDK is 26.11.0-rc0 |
| meson >= 0.58.0 | meson.build:11 | **absent** (note: stricter than DPDK's own 0.57.2) |
| ninja | INSTALL.md:69 | **absent** |
| `libbsd-dev` | INSTALL.md:74 | **absent** |
| `libdpdk.pc` on `PKG_CONFIG_PATH` | INSTALL.md:66,91-95 | **absent** |
| `intel_iommu=on` for vfio-pci | INSTALL.md:121-122 | no IOMMU in guest; also an Intel-specific flag on an AMD host |
| root for `tools/setup.sh`, `modprobe uio` | INSTALL.md:169,173 | root available, `uio` module not loaded |
| 132x42 terminal | INSTALL.md:129 | n/a |

### trex-core

| requirement | source | host state |
|---|---|---|
| gcc (repo carries `--gcc6` / `--gcc7` / `--gcc8` switches, `linux_dpdk/ws_main.py:164-171`) | ws_main.py | **absent** |
| waf build system (vendored, `waf-2.0.21`) | linux_dpdk/ | present in repo |
| python for waf | ws_main.py:1 | **met** |
| OFED for Mellanox PMD, else `--no-mlx` | ws_main.py:713 | no Mellanox NIC |
| physical DPDK-supported NIC (1/2.5/10/25/40/50/100G) | README.asciidoc:46 | **none bindable** |
| build documentation | README.asciidoc:163-165 points to the GitHub wiki | the wiki is not in the clone |

### AnchorHash (`anchorhash-cpp`)

| requirement | source | host state |
|---|---|---|
| SSE4.2 `CRC32` instruction | README.md "System Requirements"; `misc/crc32c_sse42_u64.h` | **met** (`sse4_2` present) |
| `make` plus a C++ compiler | README.md "Try it"; `tests/speed/Makefile`, `tests/balance/Makefile` | **both absent** |
| python for the plotting scripts | `tests/speed/speed_test.py`, `speed_test_plots.py` | python3 present; matplotlib/numpy not checked |

### Beamer

| requirement | source | host state |
|---|---|---|
| `beamer-mod` builds against `KDIR := ../mptcp` | `beamer-mod/Makefile:3` | the `Beamer-LB/mptcp` kernel fork was **not cloned**; `/lib/modules/$(uname -r)/build` also absent |
| kernel module build toolchain | `beamer-mod/Kbuild` | no compiler, no kernel headers |
| `beamer-click` needs a Click/FastClick tree | no build system in repo | `Beamer-LB/fastclick` **not cloned** |
| `beamer-ctrl` needs Java + Ant (NetBeans project) | `build.xml`, `nbproject/build-impl.xml` | not checked, Java not surveyed |
| `beamer-p4` is `beamer.p4.php`, needs PHP to render then a P4 compiler | file extension | neither present |

## Summary of host-level blockers

1. No compiler of any kind. Every C, C++, Go, and BPF target is unbuildable as-is.
2. No build systems: no make, cmake, meson, or ninja.
3. Zero hugepages reserved, so no DPDK EAL init.
4. No IOMMU groups and no vfio/uio loaded, so no NIC can be bound to a userspace driver.
5. No physical NIC visible inside WSL2; only the synthetic `hv_netvsc` device.
6. AF_XDP zero-copy unavailable on both relevant drivers, established from source.
7. One NIC, one host: no generator/DUT separation.
8. `perf` not installed; `perf_event_paranoid=2`; `max_precise=0`.
9. `eth0` MTU is 1280.
