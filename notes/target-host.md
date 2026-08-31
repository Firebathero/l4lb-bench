# Target host spec

`env/host.md` lists what the current machine cannot do. This turns that list into
something you can actually buy, and says what each option unblocks.

Written 2026-08-31.

## The headline constraint

**AWS ENA and Azure MANA do not support AF_XDP zero-copy.** GCP's gve does, and
so does virtio_net. This is not recalled, it is read out of the drivers:

| driver | where you get it | `ndo_bpf` | `ndo_xdp_xmit` | `ndo_xsk_wakeup` | AF_XDP zero-copy |
|---|---|---|---|---|---|
| `ena` | AWS (most instance types) | yes | yes | **no** | **no** |
| `mana` | Azure (accelerated networking) | yes | yes | **no** | **no** |
| `gve` | GCP | yes | yes | yes | **yes** |
| `virtio_net` | KVM guests (Hetzner, DO, Linode, self-hosted) | yes | yes | yes | **yes** |
| `ixgbe` | Intel X520/X540/X550 | yes | yes | yes | **yes** |
| `i40e` | Intel X710/XL710 | yes | yes | yes | **yes** |
| `ice` | Intel E810 | yes | yes | yes | **yes** |
| `igb` / `igc` | Intel I210/I225/I226 | yes | yes | yes | **yes** |
| `mlx5` | NVIDIA/Mellanox CX-4/5/6/7 | yes | yes | yes | **yes** |
| `bnxt` | Broadcom NetXtreme | yes | yes | **no** | **no** |
| `mlx4` | Mellanox CX-3 | yes | **no** | **no** | **no** |
| `nfp` | Netronome Agilio | yes | **no** | yes | yes |
| `veth` | virtual pairs, netns testbeds | yes | yes | **no** | **no** |
| `hv_netvsc` | **current host**, WSL2 / Hyper-V | yes | yes | **no** | **no** |
| `r8169` | **current host**, physical RTL8125 | **no** | **no** | **no** | none at all |

Method: `.ndo_bpf`, `.ndo_xdp_xmit` and `.ndo_xsk_wakeup` grepped out of each
driver's `net_device_ops` in `torvalds/linux` master. Reproduce, or re-run
against a specific kernel:

```bash
harness/collect/driver_xdp_matrix.sh           # master
harness/collect/driver_xdp_matrix.sh v6.12     # a pinned tag
```

Declared `xdp_features` corroborate. `gve`, `virtio_net`, `ixgbe`, `ice` and
`mlx5` all advertise `NETDEV_XDP_ACT_XSK_ZEROCOPY`; `ena`, `mana` and `bnxt`
advertise only `BASIC` and `REDIRECT` (plus `RX_SG` on bnxt).

One discrepancy, flagged not resolved: `ena` registers `.ndo_xdp_xmit` but does
not list `NETDEV_XDP_ACT_NDO_XMIT` in its declared features. Worth confirming on
real hardware before relying on ENA as a redirect target.

## What each capability requires

| capability | requirement | met by |
|---|---|---|
| katran XDP datapath | kernel 5.6+, clang 6+, any `ndo_bpf` driver | almost anything |
| cilium XDP datapath | kernel 5.10+, `CONFIG_DEBUG_INFO_BTF=y`, clang, Go | almost anything |
| XDP driver mode (not generic) | driver with `ndo_bpf` | anything except `r8169` |
| AF_XDP zero-copy | driver with `ndo_xsk_wakeup` | see table above |
| any DPDK (dpvs, pktgen, trex) | hugepages + `vfio-pci` + IOMMU + a PMD for the NIC | needs real or well-passed-through hardware |
| dpvs multi-core FNAT/SNAT | `rte_flow` supporting ipv4, ipv6, tcp, udp items and drop, queue actions (`dpvs/README.md:62`) | Intel ixgbe or NVIDIA mlx5, per dpvs's own verified list (`README.md:67`) |
| line-rate generation | second NIC or second host | provisioning choice |
| cycles/instructions per packet | working PMU | see below |
| cache, TLB, stall attribution | uncore PMUs and precise sampling | bare metal only |

## PMU availability is a second reason to go bare metal

On the current WSL2 host the core counters work and are exact
(`pmu-validation.md`), but `caps/max_precise = 0` and there are no uncore PMUs
(`amd_df`, `amd_l3`, `amd_umc` all absent). That is enough for cycles and
instructions per lookup. It is not enough for cache-miss or TLB attribution, or
for `perf record` with precise events.

Most cloud VMs are worse: hardware PMU is commonly not exposed to guests at all.
Bare metal gives the full set including IBS on AMD. If the study wants to explain
*why* one datapath costs more, not just *that* it does, bare metal is the
requirement.

## Options, cheapest first

### Option 1: any cloud VM, ~immediately

Unblocks: katran and cilium XDP datapaths in driver mode, `BPF_PROG_TEST_RUN`
per-packet cost, netns/veth functional testing, the full toolchain.

Does not unblock: DPDK, so **dpvs stays unmeasurable**. No AF_XDP zero-copy on
AWS or Azure. PMU likely absent, which would undo the cycle-level work.

Pick GCP or a KVM provider over AWS/Azure if you want AF_XDP zero-copy in this
tier, since `gve` and `virtio_net` have it and `ena`/`mana` do not.

### Option 2: bare metal, single dual-port NIC

Unblocks: everything in Option 1, plus DPDK, dpvs, pktgen-dpdk, trex-core, full
PMU with uncore and precise sampling, and generator/DUT separation across the two
ports with a loopback cable.

NIC: Intel X710 (`i40e`) or E810 (`ice`), or Mellanox CX-5/6 (`mlx5`). All three
are on dpvs's verified list or adjacent to it, all have `rte_flow`, all have
AF_XDP zero-copy, all have mature DPDK PMDs.

Avoid Broadcom `bnxt` (no zero-copy) and anything Realtek.

Also needs: IOMMU enabled in BIOS and on the kernel cmdline
(`intel_iommu=on` or `amd_iommu=on`), hugepages reserved, and
`isolcpus` for the DPDK lcores. dpvs's README documents
`isolcpus=1-9 default_hugepagesz=1G hugepagesz=1G hugepages=32`
(`dpvs/README.md:153`).

Providers that will sell you a specific NIC: Equinix Metal, Hetzner dedicated,
OVH. Confirm the exact NIC model before ordering; "10GbE" on a spec sheet is
not enough to know the driver.

### Option 3: two bare-metal boxes

Unblocks: everything, with a real wire between generator and DUT, which is the
only configuration where throughput and tail-latency numbers mean anything for
publication.

This is what the papers in `papers/` actually do. Maglev, Beamer, Cheetah and
SilkRoad all measure a DUT from a separate generator.

## Recommendation

**Option 2 is the right first purchase.** A single bare-metal box with a
dual-port Intel X710 or E810 unblocks every remaining item on the gap list
including dpvs, gives a real PMU, and supports generator/DUT separation across
two ports without a second machine. Option 3 only becomes necessary when the
generator itself becomes the bottleneck, which is a problem worth having later.

Option 1 is worth doing anyway if it is free or near-free, because it unblocks
the `BPF_PROG_TEST_RUN` work immediately and that does not need a NIC at all.

## What to verify on arrival

Run these before trusting anything. All are read-only.

```bash
uname -r; ls /sys/kernel/btf/vmlinux
ethtool -i <iface>                       # confirm the driver name
ip link show <iface>                     # confirm it is up
ethtool --show-features <iface> | grep -i xdp
ls /sys/kernel/iommu_groups | wc -l      # must be non-zero for DPDK
grep -i huge /proc/meminfo
cat /proc/sys/kernel/perf_event_paranoid
ls /sys/bus/event_source/devices/         # look for amd_df / amd_l3 / uncore
cat /sys/bus/event_source/devices/cpu/caps/max_precise   # want > 0
python3 harness/collect/pmu_probe.py
python3 harness/collect/pmu_linearity.py
```

The last two are the same checks used in `pmu-validation.md`, so results are
directly comparable against the current host.

To confirm AF_XDP zero-copy is genuinely active rather than silently falling back
to copy mode, bind with the `XDP_ZEROCOPY` flag: per
`Documentation/networking/af_xdp.rst:248-255` the bind fails outright if the
driver cannot do it, rather than degrading quietly.

## What this does not decide

Instance sizing, cost, and region. Also untested: whether any given provider's
bare metal exposes uncore PMUs, which varies with BIOS and with whether the
hypervisor is fully out of the path. Check `max_precise` and the event source
list on arrival rather than assuming.
