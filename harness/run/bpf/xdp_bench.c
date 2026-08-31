// Per-packet cost of katran's XDP datapath, measured with BPF_PROG_TEST_RUN.
//
// Loads repos/katran/katran/lib/bpf/balancer.bpf.c compiled unmodified, fills
// in the minimum control-plane state for a VIP to resolve to a backend, then
// feeds it a synthetic TCP SYN and times the program inside the kernel.
//
// No NIC, no hugepages, no driver binding. The only requirement is root, since
// env/host.md records unprivileged_bpf_disabled=2.
//
// Struct definitions come from katran's own header so the map ABI cannot drift.

#include <arpa/inet.h>
#include <errno.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "katran/lib/bpf/balancer_structs.h"

#define RING_SIZE 65537u   // katran kDefaultChRingSize / balancer_consts.h:55

static int quiet_print(enum libbpf_print_level lvl, const char *fmt, va_list ap) {
  if (lvl == LIBBPF_DEBUG) return 0;
  return vfprintf(stderr, fmt, ap);
}

static int cmp_u64(const void *a, const void *b) {
  unsigned long long x = *(const unsigned long long *)a;
  unsigned long long y = *(const unsigned long long *)b;
  return (x > y) - (x < y);
}

// Ethernet + IPv4 + TCP SYN destined to the VIP.
static int build_syn(unsigned char *buf, unsigned int vip, unsigned short vport,
                     unsigned int client, unsigned short cport) {
  memset(buf, 0, 64);
  struct ethhdr *eth = (struct ethhdr *)buf;
  eth->h_proto = htons(ETH_P_IP);
  memcpy(eth->h_dest,   "\x02\x00\x00\x00\x00\x01", 6);
  memcpy(eth->h_source, "\x02\x00\x00\x00\x00\x02", 6);

  struct iphdr *ip = (struct iphdr *)(buf + sizeof(*eth));
  ip->version = 4;
  ip->ihl = 5;
  ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
  ip->ttl = 64;
  ip->protocol = IPPROTO_TCP;
  ip->saddr = client;
  ip->daddr = vip;

  struct tcphdr *tcp = (struct tcphdr *)(buf + sizeof(*eth) + sizeof(*ip));
  tcp->source = htons(cport);
  tcp->dest = htons(vport);
  tcp->doff = 5;
  tcp->syn = 1;
  tcp->window = htons(65535);
  return sizeof(*eth) + sizeof(*ip) + sizeof(*tcp);
}

int main(int argc, char **argv) {
  const char *obj_path = (argc > 1) ? argv[1] : "build/balancer.bpf.o";
  int iters = (argc > 2) ? atoi(argv[2]) : 20000;

  libbpf_set_print(quiet_print);

  struct bpf_object *obj = bpf_object__open_file(obj_path, NULL);
  if (!obj) { fprintf(stderr, "open %s failed\n", obj_path); return 1; }
  if (bpf_object__load(obj)) { fprintf(stderr, "load failed\n"); return 1; }

  struct bpf_program *prog = bpf_object__find_program_by_name(obj, "balancer_ingress");
  if (!prog) { fprintf(stderr, "balancer_ingress not found\n"); return 1; }
  int pfd = bpf_program__fd(prog);
  int vip_fd   = bpf_object__find_map_fd_by_name(obj, "vip_map");
  int ring_fd  = bpf_object__find_map_fd_by_name(obj, "ch_rings");
  int reals_fd = bpf_object__find_map_fd_by_name(obj, "reals");
  if (vip_fd < 0 || ring_fd < 0 || reals_fd < 0) {
    fprintf(stderr, "map lookup failed\n"); return 1;
  }

  printf("program: balancer_ingress, %zu insns\n", bpf_program__insn_cnt(prog));
  printf("%-10s %-8s %-6s %-10s %-10s %-10s %-10s\n",
         "backends", "action", "out_b", "min_ns", "median_ns", "mean_ns", "p99_ns");
  printf("--------------------------------------------------------------------------\n");

  const unsigned int vip_addr = inet_addr("10.0.0.1");
  const unsigned short vip_port = 80;
  const int backend_counts[] = {1, 8, 64, 512};

  unsigned long long *d = malloc(sizeof(*d) * iters);
  if (!d) return 1;

  for (unsigned bi = 0; bi < sizeof(backend_counts)/sizeof(*backend_counts); bi++) {
    int nreals = backend_counts[bi];

    // reals: id -> backend address.
    // Real ids start at 1. balancer.bpf.c:150-155 treats a ring entry of 0 as
    // "uninitialised" and drops the packet, so id 0 must never be used.
    for (int i = 1; i <= nreals; i++) {
      struct real_definition rd;
      memset(&rd, 0, sizeof(rd));
      rd.dst = htonl(0x0a010000u + i);   // 10.1.0.x
      __u32 k = i;
      if (bpf_map_update_elem(reals_fd, &k, &rd, BPF_ANY)) {
        fprintf(stderr, "reals update failed: %s\n", strerror(errno)); return 1;
      }
    }

    // ch_rings for vip_num 0: round-robin the ring across ids 1..nreals
    for (unsigned int p = 0; p < RING_SIZE; p++) {
      __u32 k = p, v = 1u + (p % nreals);
      if (bpf_map_update_elem(ring_fd, &k, &v, BPF_ANY)) {
        fprintf(stderr, "ch_rings update failed at %u: %s\n", p, strerror(errno));
        return 1;
      }
    }

    // vip_map: key exactly as the datapath builds it (balancer.bpf.c:817-823)
    struct vip_definition vk;
    memset(&vk, 0, sizeof(vk));
    vk.vip = vip_addr;
    vk.port = htons(vip_port);
    vk.proto = IPPROTO_TCP;
    struct vip_meta vm;
    memset(&vm, 0, sizeof(vm));
    vm.flags = 0;
    vm.vip_num = 0;
    if (bpf_map_update_elem(vip_fd, &vk, &vm, BPF_ANY)) {
      fprintf(stderr, "vip_map update failed: %s\n", strerror(errno)); return 1;
    }

    unsigned char pkt[128], out[256];
    int plen = build_syn(pkt, vip_addr, vip_port, inet_addr("192.0.2.33"), 12345);

    LIBBPF_OPTS(bpf_test_run_opts, topts,
                .data_in = pkt, .data_size_in = plen,
                .data_out = out, .data_size_out = sizeof(out),
                .repeat = 1);

    // warmup
    for (int i = 0; i < 200; i++) bpf_prog_test_run_opts(pfd, &topts);

    __u32 action = 0, out_sz = 0;
    for (int i = 0; i < iters; i++) {
      topts.data_size_out = sizeof(out);
      if (bpf_prog_test_run_opts(pfd, &topts)) {
        fprintf(stderr, "test_run failed: %s\n", strerror(errno)); return 1;
      }
      d[i] = topts.duration;
      action = topts.retval;
      out_sz = topts.data_size_out;
    }

    // Invariant: the packet must actually be load balanced. A silently dropped
    // or passed packet takes a much shorter path and would understate the cost.
    if (action != XDP_TX) {
      fprintf(stderr,
              "INVARIANT FAILED at %d backends: action=%u, expected XDP_TX(%d).\n"
              "The packet was not load balanced, so the timing is meaningless.\n",
              nreals, action, XDP_TX);
      return 1;
    }
    if (out_sz != (__u32)plen + 20) {
      fprintf(stderr,
              "INVARIANT FAILED at %d backends: output %u bytes, expected %d "
              "(input + 20 for the IPIP outer header).\n",
              nreals, out_sz, plen + 20);
      return 1;
    }

    qsort(d, iters, sizeof(*d), cmp_u64);
    double sum = 0;
    for (int i = 0; i < iters; i++) sum += (double)d[i];
    printf("%-10d %-8s %-6u %-10llu %-10llu %-10.1f %-10llu\n",
           nreals, "XDP_TX", out_sz, d[0], d[iters/2], sum/iters,
           d[(int)(iters*0.99)]);
  }

  // ---- baseline: a non-IP frame exits at balancer.bpf.c:1177 (XDP_PASS)
  // after only the entry check and the stats bump. Subtracting it isolates the
  // parse, hash, lookup and encapsulation work from BPF_PROG_TEST_RUN overhead.
  {
    unsigned char pkt[128], out[256];
    memset(pkt, 0, sizeof(pkt));
    struct ethhdr *eth = (struct ethhdr *)pkt;
    eth->h_proto = htons(ETH_P_ARP);   // neither IP nor IPv6
    int plen = 54;

    LIBBPF_OPTS(bpf_test_run_opts, topts,
                .data_in = pkt, .data_size_in = plen,
                .data_out = out, .data_size_out = sizeof(out),
                .repeat = 1);
    for (int i = 0; i < 200; i++) bpf_prog_test_run_opts(pfd, &topts);
    __u32 action = 0;
    for (int i = 0; i < iters; i++) {
      topts.data_size_out = sizeof(out);
      if (bpf_prog_test_run_opts(pfd, &topts)) return 1;
      d[i] = topts.duration;
      action = topts.retval;
    }
    qsort(d, iters, sizeof(*d), cmp_u64);
    double sum = 0;
    for (int i = 0; i < iters; i++) sum += (double)d[i];
    printf("%-10s %-8s %-6s %-10llu %-10llu %-10.1f %-10llu\n",
           "baseline", action == XDP_PASS ? "XDP_PASS" : "?", "-",
           d[0], d[iters/2], sum/iters, d[(int)(iters*0.99)]);
    printf("\nbaseline is a non-IP frame: entry check + stats bump + return.\n"
           "Subtract it from the XDP_TX rows to isolate the load-balancing work.\n");
  }

  // ---- why repeat must be 1 -------------------------------------------------
  // katran encapsulates, so the output of run N is the input of run N+1 when
  // repeat > 1. An IPIP packet is neither TCP nor UDP, and INLINE_DECAP_IPIP is
  // not defined in this build, so subsequent runs fall through to XDP_PASS
  // (balancer.bpf.c:812) and the average collapses. Demonstrated, not asserted.
  {
    unsigned char pkt[128], out[256];
    int plen = build_syn(pkt, vip_addr, vip_port, inet_addr("192.0.2.33"), 12345);
    printf("\nrepeat-count sanity check (same packet, same maps):\n");
    printf("  %-10s %-10s %-12s %s\n", "repeat", "retval", "ns/run", "note");
    int reps[] = {1, 2, 10, 100};
    for (unsigned r = 0; r < sizeof(reps)/sizeof(*reps); r++) {
      LIBBPF_OPTS(bpf_test_run_opts, t,
                  .data_in = pkt, .data_size_in = plen,
                  .data_out = out, .data_size_out = sizeof(out),
                  .repeat = reps[r]);
      if (bpf_prog_test_run_opts(pfd, &t)) return 1;
      const char *note = (t.retval == XDP_TX) ? "load balanced"
                       : (t.retval == XDP_PASS) ? "NOT load balanced" : "?";
      printf("  %-10d %-10u %-12u %s\n", reps[r], t.retval, t.duration, note);
    }
  }

  free(d);
  bpf_object__close(obj);
  return 0;
}
