# Datapath inventory

Static source reading only. Nothing was compiled or run. All `file:line`
citations are against the pinned SHAs in `repos.md`. Paths are relative to each
repository root under `~/l4lb-bench/repos/`.

This file describes what the code does. It does not evaluate it.

---

## katran

SHA `e6f781e09144641967487f696a9a9f2e2975f4ef`.

| item | location | detail |
|---|---|---|
| fast-path entry | `katran/lib/bpf/balancer.bpf.c:1143-1144` | `SEC(PROG_SEC_NAME)` then `int balancer_ingress(struct xdp_md* ctx)`. XDP program. |
| packet hash | `katran/lib/bpf/balancer.bpf.c:25-36` | `get_packet_hash()`. IPv4 at :34 `jhash_2words(pckt->flow.src, pckt->flow.ports, INIT_JHASH_SEED)`. IPv6 at :29-31 `jhash_2words(jhash(pckt->flow.srcv6, 16, INIT_JHASH_SEED_V6), ...)`. `jhash.h` included at :15 from `katran/lib/linux_includes/`. |
| hash seeds | `katran/lib/bpf/balancer_consts.h:383,389` | `INIT_JHASH_SEED = CH_RINGS_SIZE`, `INIT_JHASH_SEED_V6 = MAX_VIPS` |
| backend selection | `katran/lib/bpf/balancer.bpf.c:87` | `get_packet_dst()`. :143 `hash = get_packet_hash(pckt, hash_16bytes) % RING_SIZE`. :144 `key = RING_SIZE * (vip_info->vip_num) + hash`. :146 `real_pos = bpf_map_lookup_elem(&ch_rings, &key)`. :159 `*real = bpf_map_lookup_elem(&reals, &key)`. |
| lookup table | `katran/lib/bpf/balancer_maps.h:81-87` | `ch_rings`, `BPF_MAP_TYPE_ARRAY`, key `__u32`, value `__u32`, `max_entries = CH_RINGS_SIZE`. Allocated by the BPF loader from this declaration; there is no runtime allocation site in the datapath. |
| table size | `katran/lib/bpf/balancer_consts.h:113,55,59` | `CH_RINGS_SIZE = MAX_VIPS * RING_SIZE`, `RING_SIZE 65537`, `MAX_VIPS 512`. Product: **33,554,944** `__u32` slots. |
| conntrack lookup | `katran/lib/bpf/balancer.bpf.c:177` | `connection_table_lookup()`. Reads at :185 `dst_lru = bpf_map_lookup_elem(lru_map, &pckt->flow)`, then :198 `*real = bpf_map_lookup_elem(&reals, &key)`. |
| conntrack lookup call site | `katran/lib/bpf/balancer.bpf.c:1044` | `connection_table_lookup(&dst, &pckt, lru_map, /*isGlobalLru=*/false)`, guarded by `!(pckt.flags & F_SYN_SET)` and `!(vip_info->flags & F_LRU_BYPASS)` at :1042-1043 |
| conntrack insert | `katran/lib/bpf/balancer.bpf.c:169-172` | inside `get_packet_dst()`: `new_dst_lru.atime = cur_time`, `new_dst_lru.pos = key`, then `bpf_map_update_elem(lru_map, &pckt->flow, &new_dst_lru, BPF_ANY)` |
| global LRU path | `katran/lib/bpf/balancer.bpf.c:271`, call site :1047-1053 | `perform_global_lru_lookup()`, `g_lru_map = bpf_map_lookup_elem(&global_lru_maps, &cpu_num)` at :278, then `connection_table_lookup(dst, pckt, g_lru_map, true)` at :295. Compiled under `#ifdef GLOBAL_LRU_LOOKUP`. |
| CH fallback | `katran/lib/bpf/balancer.bpf.c:1059` | comment "if dst is not found, route via consistent-hashing of the flow", followed by LRU-miss accounting at :1060-1074 |
| encapsulation, IPIP v4 | `katran/lib/bpf/pckt_encap.h:88` | `encap_v4()`. :99 `ip_src = create_encap_ipv4_src(pckt->flow.port16[0], pckt->flow.src)`, :101 comment "ipip encap". |
| encapsulation, IPIP v6 | `katran/lib/bpf/pckt_encap.h:42` | `encap_v6()`. :57 comment "ip(6)ip6 encap". Source built at :75 / :79 by `create_encap_ipv6_src()`. |
| encapsulation, GUE | `katran/lib/bpf/pckt_encap.h:238,296` | `gue_encap_v4()`, `gue_encap_v6()`, under `#ifdef GUE_ENCAP` at :162. Checksum helper `gue_csum()` at :164. UDP header built at :281 / :346 with `GUE_DPORT`. |
| inline decap | `katran/lib/bpf/pckt_encap.h:360,379` | `gue_decap_v4()`, `gue_decap_v6()`, under `#ifdef INLINE_DECAP_GUE` at :357 |

### Maps touched on the packet path

All declared in `katran/lib/bpf/balancer_maps.h`. Sizes resolve from
`katran/lib/bpf/balancer_consts.h`.

| map | type | key | value | max_entries | decl |
|---|---|---|---|---|---|
| `vip_map` | HASH | `struct vip_definition` | `struct vip_meta` | `MAX_VIPS` = 512 | :32-38 |
| `vip_lpm_map` | LPM_TRIE | `struct vip_lpm_key` | `struct vip_meta` | `MAX_CIDR_VIPS` = 256 | :45-51 |
| `fallback_cache` | LRU_HASH | `struct flow_key` | `struct real_pos_lru` | `DEFAULT_LRU_SIZE` = 1000 | :55-61 |
| `lru_mapping` | ARRAY_OF_MAPS | `__u32` | `__u32` | `MAX_SUPPORTED_CPUS` = 128 | :64-78 |
| `lru_mapping` inner | LRU_HASH | `struct flow_key` | `struct real_pos_lru` | `DEFAULT_LRU_SIZE` = 1000 | :72-77 |
| `ch_rings` | ARRAY | `__u32` | `__u32` | `CH_RINGS_SIZE` = 33554944 | :81-87 |
| `reals` | ARRAY | `__u32` | `struct real_definition` | `MAX_REALS` = 4096 | :90-96 |
| `reals_stats` | PERCPU_ARRAY | `__u32` | `struct lb_stats` | `MAX_REALS` = 4096 | :99-105 |
| `lru_miss_stats` | PERCPU_ARRAY | `__u32` (backend index) | `__u32` (miss count) | `MAX_REALS` = 4096 | :108-114 |
| `vip_miss_stats` | ARRAY | `__u32` | `struct vip_definition` | 1 | :116-122 |
| `stats` | PERCPU_ARRAY | `__u32` | `struct lb_stats` | `STATS_MAP_SIZE` = `MAX_VIPS * 2` = 1024 | :125-131 |
| `quic_stats_map` | PERCPU_ARRAY | `__u32` | `struct lb_quic_packets_stats` | 1 | :134-140 |
| `stable_rt_stats` | PERCPU_ARRAY | `__u32` | `struct lb_stable_rt_packets_stats` | 1 | :143-149 |
| `decap_vip_stats` | PERCPU_ARRAY | `__u32` | `struct lb_stats` | `MAX_VIPS` = 512 | :152-158 |
| `server_id_map` | HASH | `__u32` | `__u32` | `MAX_NUM_SERVER_IDS` = 1<<24 | :163-169 |
| `server_id_map` (alt build) | ARRAY | `__u32` | `__u32` | `MAX_QUIC_REALS` = 0x00fffffe | :171-177 |
| `lpm_src_v4` | LPM_TRIE | `struct v4_lpm_key` | `__u32` | `MAX_LPM_SRC` = 3000000 | :181-187 |
| `lpm_src_v6` | LPM_TRIE | | | | :189- |

Two `server_id_map` declarations exist, selected by preprocessor condition. Only
one is compiled in.

Other maps read from `balancer.bpf.c` but declared elsewhere: `global_lru_maps`
(:278), `decap_dst` (:217), `pckt_srcs` (:237, :252).

Additional constants: `DEFAULT_GLOBAL_LRU_SIZE 10000` (`balancer_consts.h:138`),
`LRU_UDP_TIMEOUT 30000000000U` nanoseconds, 30 s (:144), `MAX_PCKT_SIZE 1514`
(:258), `MAX_CONN_RATE 125000` (:337), `DEFAULT_TTL 64` (:185).

### Control plane and synchronisation

| item | location | detail |
|---|---|---|
| ring generation | `katran/lib/CHHelpers.h:50-52` | `virtual std::vector<int> generateHashRing(std::vector<Endpoint> endpoints, const uint32_t ring_size = kDefaultChRingSize) = 0;` |
| default ring size | `katran/lib/CHHelpers.h:25` | `constexpr uint32_t kDefaultChRingSize = 65537;` matching `RING_SIZE` in the BPF consts |
| algorithms | `katran/lib/CHHelpers.h:57-60` | `enum class HashFunction { Maglev, MaglevV2 };` selected via `CHFactory::make()` at :71 |
| endpoint weighting | `katran/lib/CHHelpers.h` | `struct Endpoint { uint32_t num; uint32_t weight; uint64_t hash; }` |
| table write | `katran/lib/KatranLb.cpp:1314-1338` | `KatranLb::programHashRing()`. :1326 `getMapFdByName(KatranLbMaps::ch_rings)`. :1328-1330 `keys[i] = vipNum * config_.chRingSize + chPositions[i].pos; values[i] = chPositions[i].real`. :1332 `bpfUpdateMapBatch(ch_fd, keys, values, updateSize)`. |
| call sites | `katran/lib/KatranLb.cpp:1095, :1310` | on VIP change and on real change |
| backend table write | `katran/lib/KatranLb.cpp:1232` | `updateRealsMap(raddr, real_iter->second.num, real_iter->second.flags)` |

**Synchronisation mechanism.** The control plane writes `ch_rings` through the
`bpf()` syscall as a batch update (`bpfUpdateMapBatch`, `KatranLb.cpp:1332`); the
datapath reads the same array with `bpf_map_lookup_elem` (`balancer.bpf.c:146`).
There is no lock, no RCU section, and no version or generation counter in either
side of the katran code. The map is a `BPF_MAP_TYPE_ARRAY` of `__u32`, so
consistency across a multi-slot update is whatever the kernel array map provides
per element; a reader can observe a partially applied batch. `programHashRing()`
is skipped entirely when `config_.testing` is set (`KatranLb.cpp:1321`).

Per-CPU LRU state needs no synchronisation: `lru_mapping` is an
`ARRAY_OF_MAPS` indexed by CPU (`balancer_maps.h:64-78`), and `reals_stats`,
`lru_miss_stats`, `stats` are `PERCPU_ARRAY`.

---

## cilium, XDP load-balancer path

SHA `fa4c8b8e192e5d18d0baed648d977c766afea60a`, `VERSION` `1.21.0-dev`.

| item | location | detail |
|---|---|---|
| fast-path entry | `bpf/bpf_xdp.c:325-330` | `__section_entry` / `int cil_xdp_entry(struct __ctx_buff *ctx)`, calling `bpf_clear_meta()`, `check_and_store_ip_trace_id()`, then `check_filters(ctx)` |
| filter dispatch | `bpf/bpf_xdp.c:280` | `check_filters()`, dispatching to `check_v4_lb()` (:160) and `check_v6_lb()` (:234) |
| prefilter | `bpf/bpf_xdp.c:175, :249` | `prefilter_v4()`, `prefilter_v6()` |
| tail calls | `bpf/bpf_xdp.c:130, :205` | `int tail_lb_ipv4(struct __ctx_buff *ctx)`, `int tail_lb_ipv6(...)` |
| LB entry, v4 | `bpf/lib/nodeport.h:2839` | `nodeport_lb4()`. :2857 `lb4_extract_tuple()`, :2870 `lb4_fill_key()`, :2872 `svc = lb4_lookup_service(&key, false)`, :2875 dispatch to `nodeport_svc_lb4()`. |
| LB entry, v6 | `bpf/lib/nodeport.h:1552, :1379` | `nodeport_lb6()`, `nodeport_svc_lb6()` |
| service lookup | `bpf/lib/lb.h:1768-1770` | `__lb4_lookup_service()` returning `map_lookup_elem(&cilium_lb4_services_v2, key)`. Wrapper `lb4_lookup_service()` at :1798. |
| backend lookup | `bpf/lib/lb.h:1900-1902` | `__lb4_lookup_backend()` returning `map_lookup_elem(&cilium_lb4_backends_v3, &backend_id)`. Wrapper at :1906. |
| backend-slot lookup | `bpf/lib/lb.h:1918-1933` | `__lb4_lookup_backend_slot()`, `lb4_lookup_backend_slot()`; the slot lookup reuses the services map |
| algorithm dispatch | `bpf/lib/lb.h:2010-2041` | `lb4_select_backend_id()`: custom (:2018), maglev (:2036), random (:2038), first (:2040). Default chosen by `lb_default_algorithm()` at :725. |
| hash + maglev select | `bpf/lib/lb.h:1956-1981` | `lb4_select_backend_id_maglev()`. :1979 `index = __hash_from_tuple_v4(tuple, sport, dport) % LB_MAGLEV_LUT_SIZE`. :1980 `map_array_get_32(backend_ids, index, (LB_MAGLEV_LUT_SIZE - 1) << 2)`. v6 equivalent at :1135-1156. |
| hash function | `bpf/lib/hash.h:12-17` | `__hash_from_tuple_v4()` = `jhash_3words(tuple->saddr, ((__u32)dport << 16) \| sport, tuple->nexthdr, CONFIG(hash_init4_seed))`. v6 at :25-40 using `__jhash_mix` / `__jhash_final` and `CONFIG(hash_init6_seed)`. |
| hash input note | `bpf/lib/hash.h:7-9` | in-source comment: "The daddr is explicitly excluded from the hash here in order to allow for backend selection to choose the same backend even on different service VIPs." |
| conntrack consult | `bpf/lib/lb.h:2252-2253` | inside `lb4_local()` (:2204): `ct_lazy_lookup4(map, tuple, ctx, fraginfo, l4_off, CT_SERVICE, SCOPE_REVERSE, CT_ENTRY_SVC, state, &monitor)` |
| conntrack insert | `bpf/lib/lb.h:2282` | `ct_create4(map, NULL, tuple, ctx, CT_SERVICE, state, ext_err)` on the `CT_NEW` branch (:2258). A second lookup/create pair for the forced-backend case sits at :2234-2244. |
| selection on CT miss | `bpf/lib/lb.h:2270-2277` | comment "No CT entry has been found, so select a svc endpoint", then :2272 `backend_id = lb4_select_backend_id(...)`, :2273 `backend = lb4_lookup_backend(...)`, :2277 `*new_backend = true` |
| session affinity | `bpf/lib/lb.h:2262-2269` | `lb4_affinity_backend_id_by_addr()` consulted before hashing when the service is marked affinity |
| `lb4_local` call site | `bpf/lib/nodeport.h:2701` | `ret = lb4_local(get_ct_map4(tuple), ctx, fraginfo, l4_off, ...)` inside `nodeport_svc_lb4()` (:2631) |
| rewrite / DNAT | `bpf/lib/lb.h:2048-2062` | `lb4_xlate()`. :2053 `const __be32 *new_daddr = &backend->address`. :2061-2062 `ctx_store_bytes(ctx, l3_off + offsetof(struct iphdr, daddr), new_daddr, 4, 0)`. Checksum offset resolved at :2059 by `csum_l4_offset_and_flags()`. |
| reverse NAT | `bpf/lib/lb.h:775, :860` | `__lb6_rev_nat()`, `lb6_rev_nat()`; v4 equivalents alongside |

### Maps touched on the packet path

Declared in `bpf/lib/lb.h` (LB maps) and `bpf/lib/conntrack_map.h` (CT maps).
All LB maps carry `__uint(pinning, LIBBPF_PIN_BY_NAME)`.

| map | type | key | value | max_entries symbol | decl |
|---|---|---|---|---|---|
| `cilium_lb4_services_v2` | HASH | `struct lb4_key` | `struct lb4_service` | `CILIUM_LB_SERVICE_MAP_MAX_ENTRIES` | `lb.h:277-284` |
| `cilium_lb4_backends_v3` | HASH | `__u32` | `struct lb4_backend` | `CILIUM_LB_BACKENDS_MAP_MAX_ENTRIES` | `lb.h:290-297` |
| `cilium_lb4_maglev` | HASH_OF_MAPS | `__u16` | `__u32` | `CILIUM_LB_MAGLEV_MAP_MAX_ENTRIES` | `lb.h:326-342` |
| `cilium_lb4_maglev` inner | ARRAY | `__u32` | `sizeof(__u32) * LB_MAGLEV_LUT_SIZE` | 1 | `lb.h:338-340` |
| `cilium_lb4_reverse_nat` | HASH | `__u16` | `struct lb4_reverse_nat` | `CILIUM_LB_REV_NAT_MAP_MAX_ENTRIES` | `lb.h:268-275` |
| `cilium_lb6_services_v2` | HASH | `struct lb6_key` | `struct lb6_service` | `CILIUM_LB_SERVICE_MAP_MAX_ENTRIES` | `lb.h:197-204` |
| `cilium_lb6_backends_v3` | HASH | `__u32` | `struct lb6_backend` | `CILIUM_LB_BACKENDS_MAP_MAX_ENTRIES` | `lb.h:210-217` |
| `cilium_lb6_maglev` | HASH_OF_MAPS | `__u16` | `__u32` | `CILIUM_LB_MAGLEV_MAP_MAX_ENTRIES` | `lb.h:246-261` |
| `cilium_lb6_affinity` | LRU_HASH | `struct lb6_affinity_key` | `struct lb_affinity_val` | `CILIUM_LB_AFFINITY_MAP_MAX_ENTRIES` | `lb.h:219-226` |
| `cilium_lb6_source_range` | LPM_TRIE | `struct lb6_src_range_key` | `__u8` | `LB6_SRC_RANGE_MAP_SIZE` | `lb.h:228-235` |
| `cilium_lb6_health` | LRU_HASH | `__sock_cookie` | `struct lb6_health` | `CILIUM_LB_BACKENDS_MAP_MAX_ENTRIES` | `lb.h:237-244` |
| `cilium_ct4_global` | LRU_HASH | | | `CT_MAP_SIZE_TCP` | `conntrack_map.h:105-111` |
| `cilium_ct_any4_global` | LRU_HASH | | | `CT_MAP_SIZE_ANY` | `conntrack_map.h:114-120` |
| `cilium_ct6_global` | LRU_HASH | | | `CT_MAP_SIZE_TCP` | `conntrack_map.h:11-17` |
| `cilium_ct_any6_global` | LRU_HASH | | | `CT_MAP_SIZE_ANY` | `conntrack_map.h:20-26` |
| `cilium_per_cluster_ct_tcp4` | ARRAY_OF_MAPS | | inner LRU_HASH `CT_MAP_SIZE_TCP` | 256 | `conntrack_map.h:138-145` |
| `cilium_per_cluster_ct_any4` | ARRAY_OF_MAPS | | inner LRU_HASH `CT_MAP_SIZE_ANY` | 256 | `conntrack_map.h:156-163` |

Map flags: services, maglev, and source-range maps use
`CONDITIONAL_PREALLOC | BPF_F_RDONLY_PROG_COND`. Backends and reverse-NAT use
`CONDITIONAL_PREALLOC` only. `lb.h:206-209` and :286-289 carry the same in-source
comment explaining why: "Could be read-only from datapath, but
bpf_xdp_store_bytes (unlike bpf_skb_store_bytes) does not accept MEM_RDONLY
pointers, so map values passed to ctx_store_bytes in XDP programs would be
rejected by the verifier."

`CONDITIONAL_PREALLOC` and `LRU_MEM_FLAVOR` are defined in `bpf/lib/map_defs.h:9-17`.

### Where the sizes come from

These are compile-time `#define`s emitted by the Go agent, not literals in the
BPF source.

| symbol | emitted at | source value | default |
|---|---|---|---|
| `LB_MAGLEV_LUT_SIZE` | `pkg/datapath/linux/config/config.go:321` | `cfg.MaglevConfig.TableSize` | **16381** (`pkg/maglev/maglev.go:38`, applied at :92) |
| `CILIUM_LB_SERVICE_MAP_MAX_ENTRIES` | `config.go:159` | `cfg.LBConfig.LBServiceMapEntries` | flag `bpf-lb-service-map-max` (`pkg/loadbalancer/config.go:31,153`) |
| `CILIUM_LB_BACKENDS_MAP_MAX_ENTRIES` | `config.go:160` | `cfg.LBConfig.LBBackendMapEntries` | flag `bpf-lb-service-backend-map-max` (`pkg/loadbalancer/config.go:156`) |
| `CILIUM_LB_MAGLEV_MAP_MAX_ENTRIES` | `config.go:163` | `cfg.LBConfig.LBMaglevMapEntries` | flag `bpf-lb-maglev-map-max` (`pkg/loadbalancer/config.go:172`) |
| `CT_MAP_SIZE_TCP` | `config.go:355` | `option.Config.CTMapEntriesGlobalTCP` | **524288** (`2 << 18`, `pkg/option/config.go:489`) |
| `CT_MAP_SIZE_ANY` | `config.go:356` | `option.Config.CTMapEntriesGlobalAny` | **262144** (`2 << 17`, `pkg/option/config.go:493`) |

`pkg/option/config.go:514` derives `NATMapEntriesGlobalDefault =
int((CTMapEntriesGlobalTCPDefault + CTMapEntriesGlobalAnyDefault) * 2 / 3)`.

### Control plane and synchronisation

The control plane is the Go agent. Maglev table generation lives in
`pkg/maglev/maglev.go`; map sizing flags in `pkg/loadbalancer/config.go`; the
generated C header defines in `pkg/datapath/linux/config/config.go`.

**Note on repository layout at this SHA:** there is no `pkg/maps/lbmap`
directory. `pkg/maps/` contains `ctmap`, `nat`, `ipcache`, `lxcmap`,
`policymap`, and others, but LB map handling is not there. The exact Go writer
for `cilium_lb4_services_v2` and `cilium_lb4_backends_v3` was not traced;
recorded as a gap rather than guessed.

**Synchronisation mechanism.** All LB maps are pinned by name
(`LIBBPF_PIN_BY_NAME`), so the agent opens the pinned fd and updates entries
through the `bpf()` syscall while the XDP program reads them. Services, maglev,
and source-range maps are marked `BPF_F_RDONLY_PROG_COND`, which makes them
read-only from the BPF side outside of tests. There is no explicit lock between
agent and datapath.

The maglev table is a `BPF_MAP_TYPE_HASH_OF_MAPS` keyed by `__u16`
(`lb.h:326-342`), whose value is an inner single-element array holding
`LB_MAGLEV_LUT_SIZE` backend ids. Because the outer map holds map references,
updating a service's table replaces the inner map rather than mutating slots
in place.

The datapath's own consistency mechanism is the CT_SERVICE conntrack entry:
`lb4_local()` looks up CT first (`lb.h:2252`) and only hashes on `CT_NEW`
(:2258, :2272), so an established flow keeps its backend from CT state rather
than re-deriving it. `lb.h:2286-2290` carries the in-source comment for the
`CT_REPLY` path: "If the lookup fails it means the user deleted the backend out
from underneath us. To resolve this fall back to hash. If this is a TCP session
we are likely to get a TCP RST."

---

## dpvs

SHA `4582d20dc6cc16ab123c3b72e245e3ec9a47965b`, tag `v1.10.2`.

| item | location | detail |
|---|---|---|
| rx poll loop | `src/netif.c:1827` | `nrx = rte_eth_rx_burst(pid, qconf->id, qconf->mbufs, NETIF_MAX_PKT_BURST)` |
| burst size | `include/netif.h:60` | `#define NETIF_MAX_PKT_BURST 32` |
| isolated rx variant | `src/netif.c:1760-1761` | `rte_eth_rx_burst()` into a `rte_ring` for isolated receive lcores |
| per-burst dispatch | `src/netif.c:2577` | `void lcore_process_packets(struct rte_mbuf **mbufs, lcoreid_t cid, uint16_t count, bool pkts_from_ring)`, called at :2656 |
| per-packet dispatch | `src/netif.c:2419`, call site :2612 | `static int netif_deliver_mbuf(struct netif_port *dev, lcoreid_t cid, struct rte_mbuf *mbuf, bool pkts_from_ring)` |
| IPVS hook registration | `src/ipvs/ip_vs_core.c:1134-1158` | `static struct inet_hook_ops dp_vs_ops[]`, four entries, all `.hooknum = INET_HOOK_PRE_ROUTING` |
| fast-path entry | `src/ipvs/ip_vs_core.c:934` | `static int __dp_vs_in(void *priv, struct rte_mbuf *mbuf, const struct inet_hook_state *state, int af)`; wrappers `dp_vs_in()` at :1075 and `dp_vs_in6()` at :1081 |
| pre-routing hook | `src/ipvs/ip_vs_core.c:1087` | `__dp_vs_pre_routing()`, wrappers at :1122 and :1128 |
| lcore identity | `src/ipvs/ip_vs_core.c:944` | `cid = peer_cid = rte_lcore_id()` |
| header parse | `src/ipvs/ip_vs_core.c:950` | `dp_vs_fill_iphdr(af, mbuf, &iph)` |
| ICMP relate | `src/ipvs/ip_vs_core.c:918`, call site :953 | `dp_vs_in_icmp()` |
| conn lookup / schedule | `src/ipvs/ip_vs_core.c:1005` | `prot->conn_sched(prot, &iph, mbuf, &conn, &verdict)`, per-protocol callback |
| scheduling | `src/ipvs/ip_vs_core.c:276` | `struct dp_vs_conn *dp_vs_schedule(struct dp_vs_service *svc, ...)`; :297 `dest = svc->scheduler->schedule(svc, mbuf, iph)`; :349 `conn = dp_vs_conn_new(mbuf, iph, &param, dest, flags)` |
| persistence templates | `src/ipvs/ip_vs_core.c:118, :127, :142, :151` | template connections scheduled and created separately |
| state transition | `src/ipvs/ip_vs_core.c:1062` | `err = prot->state_trans(prot, conn, mbuf, dir)` |
| transmit | `src/ipvs/ip_vs_core.c:1070, :1072` | `xmit_inbound(mbuf, prot, conn)` or `xmit_outbound(mbuf, prot, conn)` |

### Connection tracking structure

| item | location | detail |
|---|---|---|
| table bits | `src/ipvs/ip_vs_conn.c:41-42` | `#define DPVS_CONN_TBL_BITS 20`, `#define DPVS_CONN_TBL_SIZE (1 << DPVS_CONN_TBL_BITS)` = **1,048,576** buckets |
| table storage | `src/ipvs/ip_vs_conn.c:57, :72` | `#define this_conn_tbl (RTE_PER_LCORE(dp_vs_conn_tbl))`, `static RTE_DEFINE_PER_LCORE(struct list_head *, dp_vs_conn_tbl);`. Hash table of `list_head` chains, **one table per lcore**. |
| template table | `src/ipvs/ip_vs_conn.c:79` | `static rte_spinlock_t dp_vs_ct_lock;` guarding the shared `dp_vs_ct_tbl` |
| per-lcore lock | `src/ipvs/ip_vs_conn.c:74` | `static RTE_DEFINE_PER_LCORE(rte_spinlock_t, dp_vs_conn_lock);` |
| per-lcore counter | `src/ipvs/ip_vs_conn.c:81` | `static RTE_DEFINE_PER_LCORE(uint32_t, dp_vs_conn_count);` |
| conn allocation | `src/ipvs/ip_vs_conn.c:88, :98, :123` | `static struct rte_mempool *dp_vs_conn_cache[DPVS_MAX_SOCKET];`, `rte_mempool_get()`, `rte_mempool_put()`. Pool sizing from `conn_pool_size` / `conn_pool_cache` at :50-51. |
| hash key | `src/ipvs/ip_vs_conn.c:208-235` | `dp_vs_conn_hashkey()`. IPv4: `rte_jhash_3words(saddr, daddr, (sport << 16) \| dport, dp_vs_conn_rnd) & mask`. IPv6: builds a 9-word vector (ports, 16 B src, 16 B dst) then `rte_jhash_32b(vect, 9, dp_vs_conn_rnd) & mask`. |
| insert | `src/ipvs/ip_vs_conn.c:237-268` | `__dp_vs_conn_hash()`. Two hashes per connection, inbound at :244 and outbound at :249. Templates take `dp_vs_ct_lock` (:256-259); ordinary connections `list_add()` into `this_conn_tbl` with no lock (:261-262). Refcount bump at :266 `rte_atomic32_inc(&conn->refcnt)`. |
| insert wrapper | `src/ipvs/ip_vs_conn.c:271-292` | `dp_vs_conn_hash()`; optional `rte_spinlock_lock(&this_conn_lock)` at :280 and :286 under `#ifdef CONFIG_DPVS_IPVS_CONN_LOCK`. Skips one-packet connections at :274. |
| insert call sites | `src/ipvs/ip_vs_conn.c:701, :934` | |

### Schedulers

| scheduler | file | detail |
|---|---|---|
| registration | `src/ipvs/ip_vs_sched.c:172, :204` | `register_dp_vs_scheduler()`, `list_add(&scheduler->n_list, &dp_vs_schedulers)`; lookup by name at :148 |
| round robin | `src/ipvs/ip_vs_rr.c` | |
| weighted round robin | `src/ipvs/ip_vs_wrr.c` | |
| weighted least connection | `src/ipvs/ip_vs_wlc.c` | |
| failover | `src/ipvs/ip_vs_fo.c` | |
| consistent hash | `src/ipvs/ip_vs_conhash.c` | `#define REPLICA 160` at :36. Ring built by `conhash_init()` at :356, nodes added with `conhash_set_node(p_node, iden, weight / weight_gcd * REPLICA)` at :182 and :230, then `conhash_add_node()` at :185 and :232. Per-packet lookup `conhash_lookup(conhash, str)` at :140. Vendored library under `src/ipvs/libconhash/`. |
| maglev | `src/ipvs/ip_vs_mh.c` | See below. |

Maglev table sizing, `src/ipvs/ip_vs_mh.c:50-60`:

```
static int primes[] = {251, 509, 1021, 2039, 4093,
    8191, 16381, 32749, 65521, 131071};

#ifndef CONFIG_DP_VS_MH_TAB_INDEX
#define CONFIG_DP_VS_MH_TAB_INDEX   12
#endif
#define DP_VS_MH_TAB_BITS           (CONFIG_DP_VS_MH_TAB_INDEX / 2)
#define DP_VS_MH_TAB_INDEX          (CONFIG_DP_VS_MH_TAB_INDEX - 8)
#define DP_VS_MH_TAB_SIZE           primes[DP_VS_MH_TAB_INDEX]
```

With the default `CONFIG_DP_VS_MH_TAB_INDEX = 12`: `DP_VS_MH_TAB_INDEX = 4`, so
`DP_VS_MH_TAB_SIZE = primes[4] = ` **4093** entries, and
`DP_VS_MH_TAB_BITS = 6`.

Allocation site: `src/ipvs/ip_vs_mh.c:162`
`table = rte_calloc(NULL, BITS_TO_LONGS(DP_VS_MH_TAB_SIZE), ...)`.
Permutation state per destination is `struct dp_vs_mh_dest_setup { unsigned int
offset; unsigned int skip; unsigned int perm; int turns; }` at :43-48. The
population loop runs :170-203, advancing `ds->perm` by `skip` modulo
`DP_VS_MH_TAB_SIZE` (:185-188) and terminating at :203 when `n ==
DP_VS_MH_TAB_SIZE`.

The kernel's own IPVS on this host uses the same knob name:
`CONFIG_IP_VS_MH_TAB_INDEX=12` (see `env/host.md`).

### Rewrite and encapsulation

All in `src/ipvs/ip_vs_xmit.c`. DPVS rewrites headers rather than encapsulating,
except in the tunnel forwarding mode.

| forwarding mode | entry | per-family implementations |
|---|---|---|
| FNAT (full NAT) | `dp_vs_xmit_fnat()` :698 | `__dp_vs_xmit_fnat4()` :392, `__dp_vs_xmit_fnat6()` :499, `__dp_vs_xmit_fnat64()` :598 |
| FNAT fast path | `dp_vs_fast_xmit_fnat()` :150 | `__dp_vs_fast_xmit_fnat4()` :36, `__dp_vs_fast_xmit_fnat6()` :97 |
| FNAT outbound | `dp_vs_fast_outxmit_fnat()` :272 | `__dp_vs_fast_outxmit_fnat4()` :159, `__dp_vs_fast_outxmit_fnat6()` :219 |
| FNAT outbound slow | | `__dp_vs_out_xmit_fnat4()` :719, `__dp_vs_out_xmit_fnat6()` :822, `__dp_vs_out_xmit_fnat46()` :916 |
| DR (direct routing) | `dp_vs_xmit_dr()` :1327 | `__dp_vs_xmit_dr4()` :1231, `__dp_vs_xmit_dr6()` :1280 |
| SNAT | `dp_vs_xmit_snat()` :1500 | `__dp_vs_xmit_snat4()` :1339, `__dp_vs_xmit_snat6()` :1424 |
| SNAT outbound | | `__dp_vs_out_xmit_snat4()` :1512, `__dp_vs_out_xmit_snat6()` :1674 |
| NAT fast path | | `dp_vs_fast_xmit_nat()` :1584, `dp_vs_fast_outxmit_nat()` :1629 |

Tunnel mode is referenced in the docs (`README.md`) as a forwarding mode;
`ip_gre.c`, `ip_tunnel.c`, and `ipip.c` under `src/` carry the encapsulation
implementations.

### Structures touched per packet

| structure | type | size | location |
|---|---|---|---|
| per-lcore conn hash table | array of `struct list_head` | `1 << 20` = 1048576 buckets, one table per lcore | `ip_vs_conn.c:41-42, :57, :72` |
| shared template table `dp_vs_ct_tbl` | array of `struct list_head`, spinlock-guarded | same bucket count | `ip_vs_conn.c:79`, used :256-259 |
| conn object pool | `rte_mempool`, per NUMA socket | `conn_pool_size` / `conn_pool_cache` | `ip_vs_conn.c:50-51, :88` |
| maglev lookup table | `rte_calloc` bit array | `DP_VS_MH_TAB_SIZE` = 4093 default | `ip_vs_mh.c:60, :162` |
| conhash ring | vendored libconhash | `REPLICA` = 160 virtual nodes per unit weight | `ip_vs_conhash.c:36, :182` |
| rx burst buffer | `struct rte_mbuf *[NETIF_MAX_PKT_BURST]` | 32 | `netif.h:60`, `netif.c:1752, :1827` |
| service / dest tables | | | `src/ipvs/ip_vs_service.c`, `ip_vs_dest.c` |

### Control plane and synchronisation

DPVS runs one master lcore and N worker lcores in a DPDK process. Configuration
arrives over setsockopt and is fanned out to worker lcores as messages.

| item | location | detail |
|---|---|---|
| sockopt to message mapping | `src/ipvs/ip_vs_service.c:829-848` | `set_opt_so2msg()` maps `SOCKOPT_*` to `MSG_TYPE_SVC_SET_FLUSH / ZERO / ADD / EDIT / DEL / ADDDEST / DELDEST / EDITDEST` |
| service update path | `src/ipvs/ip_vs_service.c:861, :881-890` | `dp_vs_service_set()` builds a `struct dpvs_msg *` and calls `multicast_msg_send(msg, DPVS_MSG_F_ASYNC, NULL)` |
| dest update path | `src/ipvs/ip_vs_dest.c:477, :484` | `msg_make(..., DPVS_MSG_MULTICAST, rte_lcore_id(), ...)` then `multicast_msg_send(msg, DPVS_MSG_F_ASYNC, NULL)`. Also :564-573 and :766-770. |
| worker to master | `src/ipvs/ip_vs_dest.c:641-648, :673-680` | `msg_make(..., DPVS_MSG_UNICAST, ...)` then `msg_send(msg, g_master_lcore_id, DPVS_MSG_F_ASYNC, NULL)` |
| synchronous gather | `src/ipvs/ip_vs_dest.c:833-838` | `MSG_TYPE_AGENT_GET_DESTS` multicast with a reply (`multicast_msg_send(msg, 0, &reply)`) |
| read-back callbacks | `src/ipvs/ip_vs_service.c:1005, :1055, :1082` | `dp_vs_services_get_uc_cb()`, `dp_vs_service_get_uc_cb()`, `dp_vs_dests_get_uc_cb()` |
| message subsystem | `src/ctrl.c` | `msg_make`, `msg_send`, `multicast_msg_send`, `struct dpvs_msg_type` |

**Synchronisation mechanism.** There is no shared lock between the control
plane and the per-lcore datapath for service and destination updates. Instead
the master lcore multicasts a `dpvs_msg` to every worker lcore, mostly with
`DPVS_MSG_F_ASYNC`, and each worker applies the change to its own copy on its
own thread. Connection state is per-lcore by construction
(`RTE_DEFINE_PER_LCORE(dp_vs_conn_tbl)`, `ip_vs_conn.c:72`), so ordinary
connection insert and lookup need no locking (`ip_vs_conn.c:261-262`). Two
exceptions carry real locks:

1. persistence templates live in the shared `dp_vs_ct_tbl` guarded by the global
   `dp_vs_ct_lock` (`ip_vs_conn.c:79`, taken at :256-259)
2. a per-lcore spinlock `this_conn_lock` is compiled in only under
   `CONFIG_DPVS_IPVS_CONN_LOCK` (`ip_vs_conn.c:74`, taken at :280 / :286)

Connection lifetime is refcounted with `rte_atomic32_inc(&conn->refcnt)`
(`ip_vs_conn.c:266`) and matching `dp_vs_conn_put()` calls on every return path
in `__dp_vs_in()`.

---

## Cross-project summary of the selection mechanism

Presented as read from the source, without comparison.

| | katran | cilium XDP | dpvs |
|---|---|---|---|
| hash | `jhash_2words` over src addr + ports | `jhash_3words` over saddr, ports, nexthdr, seed; daddr excluded | `rte_jhash_3words` (conn table) plus scheduler-specific hash |
| table | `ch_rings` flat `ARRAY`, `MAX_VIPS * RING_SIZE` = 33554944 slots | `HASH_OF_MAPS`, one inner array per service, `LB_MAGLEV_LUT_SIZE` = 16381 default | maglev bit table 4093 default, or conhash ring at 160 replicas per weight unit |
| flow state | per-CPU `LRU_HASH`, 1000 entries, plus global LRU 10000 | `LRU_HASH` CT, `CT_MAP_SIZE_TCP` = 524288 default | per-lcore chained hash, `1 << 20` buckets, mempool-backed objects |
| output | IPIP or GUE encapsulation | header rewrite (DNAT) via `ctx_store_bytes` | header rewrite (FNAT/SNAT/NAT) or direct routing or tunnel |
| control-plane sync | batch `bpf()` map update, no lock or version counter | pinned map update via `bpf()`, inner-map replacement for maglev | async multicast `dpvs_msg` to per-lcore copies |
