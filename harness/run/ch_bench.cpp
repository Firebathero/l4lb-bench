// Consistent-hashing comparison across the real upstream implementations.
//
// Nothing here reimplements an algorithm. Every mapping decision is made by
// upstream code compiled from repos/ at the SHAs pinned in notes/repos.md:
//
//   katran MaglevHash    repos/katran/katran/lib/MaglevHash.cpp
//   katran MaglevHashV2  repos/katran/katran/lib/MaglevHashV2.cpp
//   AnchorHash           repos/anchorhash-cpp/AnchorHashQre.cpp
//   dpvs libconhash      repos/dpvs/src/ipvs/libconhash/*.c
//
// This file only feeds them keys and counts what comes back.
//
// Measured per (implementation, backend count, table size, weighting):
//   disruption  fraction of keys that change backend when one backend is removed
//   ideal       share that sat on the removed backend, the unavoidable minimum
//   excess      disruption - ideal, i.e. gratuitous remapping
//   balance     peak-to-mean, min-to-mean and coefficient of variation
//   build       time to construct the lookup structure from scratch
//   update      time to apply a single backend removal
//   lookup      cycles and instructions per lookup, from the hardware PMU

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "katran/lib/CHHelpers.h"
#include "AnchorHashQre.hpp"
extern "C" {
#include "conhash.h"
}

// ---------------------------------------------------------------- PMU reader
// perf_event_open path validated in notes/pmu-validation.md: counters are
// proportional to work (0.0% scaling error) and repeatable (0.000% spread).
class Counter {
 public:
  explicit Counter(uint64_t config) {
    perf_event_attr attr{};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = config;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.disabled = 1;
    fd_ = (int)syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
  }
  ~Counter() { if (fd_ >= 0) close(fd_); }
  bool ok() const { return fd_ >= 0; }
  void start() {
    if (fd_ < 0) return;
    ioctl(fd_, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
  }
  uint64_t stop() {
    if (fd_ < 0) return 0;
    ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
    uint64_t v = 0;
    if (read(fd_, &v, sizeof(v)) != sizeof(v)) return 0;
    return v;
  }
 private:
  int fd_ = -1;
};

// ------------------------------------------------------------------ key set
static inline uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

static const uint64_t KEY_SEED = 0x5EED1234ABCD0001ULL;
static const uint64_t AH_SEED  = 0x00C0FFEE00C0FFEEULL;

// Skewed weighting: backend i gets weight (i % 4) + 1, so weights cycle
// 1,2,3,4. This is what separates MaglevHash from MaglevHashV2; under equal
// weights the two produce identical rings.
static inline uint32_t weight_of(int i, bool skewed) {
  return skewed ? (uint32_t)((i % 4) + 1) : 1u;
}

// ------------------------------------------------------------------ metrics
struct Row {
  std::string impl;
  std::string weights;
  int backends = 0;
  uint32_t table = 0;
  double disruption = 0, excess = 0, ideal = 0;
  double peak_to_mean = 0, min_to_mean = 0, cv = 0;
  double build_us = 0, update_us = 0;
  double cycles_per_lookup = 0, ins_per_lookup = 0, ns_per_lookup = 0;
};

static void balance_stats(const std::vector<int>& assign, int n,
                          double& peak, double& mn, double& cv) {
  std::vector<uint64_t> cnt(n, 0);
  for (int a : assign) if (a >= 0 && a < n) cnt[a]++;
  double mean = (double)assign.size() / n, mx = 0, mi = 1e18, ss = 0;
  for (uint64_t c : cnt) {
    mx = std::max(mx, (double)c);
    mi = std::min(mi, (double)c);
    ss += ((double)c - mean) * ((double)c - mean);
  }
  peak = mx / mean;
  mn = mi / mean;
  cv = std::sqrt(ss / n) / mean;
}

static double disruption_of(const std::vector<int>& before,
                            const std::vector<int>& after, int removed,
                            double& ideal_out) {
  size_t moved = 0, on_removed = 0;
  for (size_t i = 0; i < before.size(); i++) {
    if (before[i] == removed) on_removed++;
    if (before[i] != after[i]) moved++;
  }
  ideal_out = (double)on_removed / before.size();
  return (double)moved / before.size();
}

// ------------------------------------------------------------------- katran
static std::vector<katran::Endpoint> endpoints(int n, bool skewed) {
  std::vector<katran::Endpoint> v;
  v.reserve(n);
  for (int i = 0; i < n; i++)
    v.push_back({(uint32_t)i, weight_of(i, skewed),
                 splitmix64(0xE1DE0000ULL + (uint64_t)i)});
  return v;
}

static Row run_maglev(katran::HashFunction fn, const char* name, int n,
                      uint32_t ring_size, bool skewed,
                      const std::vector<uint64_t>& keys) {
  Row r;
  r.impl = name;
  r.weights = skewed ? "skewed" : "equal";
  r.backends = n;
  r.table = ring_size;
  auto ch = katran::CHFactory::make(fn);

  auto t0 = std::chrono::steady_clock::now();
  auto ring = ch->generateHashRing(endpoints(n, skewed), ring_size);
  auto t1 = std::chrono::steady_clock::now();
  r.build_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

  std::vector<int> before(keys.size());
  Counter cyc(PERF_COUNT_HW_CPU_CYCLES), ins(PERF_COUNT_HW_INSTRUCTIONS);
  cyc.start(); ins.start();
  auto l0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < keys.size(); i++)
    before[i] = ring[keys[i] % ring_size];
  auto l1 = std::chrono::steady_clock::now();
  uint64_t c = cyc.stop(), ic = ins.stop();
  r.cycles_per_lookup = (double)c / keys.size();
  r.ins_per_lookup = (double)ic / keys.size();
  r.ns_per_lookup =
      std::chrono::duration<double, std::nano>(l1 - l0).count() / keys.size();

  balance_stats(before, n, r.peak_to_mean, r.min_to_mean, r.cv);

  int trials = std::min(n, 8);
  double dsum = 0, isum = 0, usum = 0;
  for (int t = 0; t < trials; t++) {
    int victim = (int)((uint64_t)t * n / trials);
    auto reduced = endpoints(n, skewed);
    reduced.erase(reduced.begin() + victim);

    auto u0 = std::chrono::steady_clock::now();
    auto ring2 = ch->generateHashRing(reduced, ring_size);  // full regeneration
    auto u1 = std::chrono::steady_clock::now();
    usum += std::chrono::duration<double, std::micro>(u1 - u0).count();

    std::vector<int> after(keys.size());
    for (size_t i = 0; i < keys.size(); i++)
      after[i] = ring2[keys[i] % ring_size];
    double ideal = 0;
    dsum += disruption_of(before, after, victim, ideal);
    isum += ideal;
  }
  r.disruption = dsum / trials;
  r.ideal = isum / trials;
  r.excess = r.disruption - r.ideal;
  r.update_us = usum / trials;
  return r;
}

// --------------------------------------------------------------- anchorhash
// AnchorHashQre has no weight concept: every working bucket is equivalent.
static Row run_anchorhash(int n, const std::vector<uint64_t>& keys) {
  Row r;
  r.impl = "anchorhash";
  r.weights = "equal";
  r.backends = n;
  r.table = (uint32_t)n;  // anchor capacity == working set

  auto t0 = std::chrono::steady_clock::now();
  AnchorHashQre ah((uint32_t)n, (uint32_t)n);
  auto t1 = std::chrono::steady_clock::now();
  r.build_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

  std::vector<int> before(keys.size());
  Counter cyc(PERF_COUNT_HW_CPU_CYCLES), ins(PERF_COUNT_HW_INSTRUCTIONS);
  cyc.start(); ins.start();
  auto l0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < keys.size(); i++)
    before[i] = (int)ah.ComputeBucket(keys[i], AH_SEED);
  auto l1 = std::chrono::steady_clock::now();
  uint64_t c = cyc.stop(), ic = ins.stop();
  r.cycles_per_lookup = (double)c / keys.size();
  r.ins_per_lookup = (double)ic / keys.size();
  r.ns_per_lookup =
      std::chrono::duration<double, std::nano>(l1 - l0).count() / keys.size();

  balance_stats(before, n, r.peak_to_mean, r.min_to_mean, r.cv);

  int trials = std::min(n, 8);
  double dsum = 0, isum = 0, usum = 0;
  for (int t = 0; t < trials; t++) {
    int victim = (int)((uint64_t)t * n / trials);
    auto u0 = std::chrono::steady_clock::now();
    ah.UpdateRemoval((uint32_t)victim);      // in-place, no regeneration
    auto u1 = std::chrono::steady_clock::now();
    usum += std::chrono::duration<double, std::micro>(u1 - u0).count();

    std::vector<int> after(keys.size());
    for (size_t i = 0; i < keys.size(); i++)
      after[i] = (int)ah.ComputeBucket(keys[i], AH_SEED);
    double ideal = 0;
    dsum += disruption_of(before, after, victim, ideal);
    isum += ideal;
    ah.UpdateNewBucket();                    // restore the bucket just removed
  }
  r.disruption = dsum / trials;
  r.ideal = isum / trials;
  r.excess = r.disruption - r.ideal;
  r.update_us = usum / trials;
  return r;
}

// ------------------------------------------------------------ dpvs conhash
// dpvs sets replicas = weight / weight_gcd * REPLICA with REPLICA = 160
// (repos/dpvs/src/ipvs/ip_vs_conhash.c:36,182). Equal weights give 160 each.
static const unsigned DPVS_REPLICA = 160;

// conhash_fini() calls (*node_fini)(node) once a node's replica count reaches
// zero (conhash.c:47). Passing NULL there jumps to address 0. dpvs supplies a
// real finaliser that frees its own wrapper; our nodes are owned by a
// std::vector, so the correct finaliser here is a no-op.
static void node_fini_noop(struct node_s*) {}

static Row run_conhash(int n, bool skewed, const std::vector<uint64_t>& keys) {
  Row r;
  r.impl = "dpvs-conhash";
  r.weights = skewed ? "skewed" : "equal";
  r.backends = n;
  r.table = DPVS_REPLICA;

  std::vector<node_s> nodes(n);
  struct conhash_s* ch = conhash_init(NULL);

  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; i++) {
    char iden[64];
    snprintf(iden, sizeof(iden), "backend-%d", i);
    conhash_set_node(&nodes[i], iden, weight_of(i, skewed) * DPVS_REPLICA);
    nodes[i].data = (void*)(intptr_t)(i + 1);  // +1 so backend 0 is not NULL
    conhash_add_node(ch, &nodes[i]);
  }
  auto t1 = std::chrono::steady_clock::now();
  r.build_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

  std::vector<int> before(keys.size());
  Counter cyc(PERF_COUNT_HW_CPU_CYCLES), ins(PERF_COUNT_HW_INSTRUCTIONS);
  cyc.start(); ins.start();
  auto l0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < keys.size(); i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)keys[i]);
    const struct node_s* nd = conhash_lookup(ch, buf);
    before[i] = nd ? (int)(intptr_t)nd->data - 1 : -1;
  }
  auto l1 = std::chrono::steady_clock::now();
  uint64_t c = cyc.stop(), ic = ins.stop();
  r.cycles_per_lookup = (double)c / keys.size();
  r.ins_per_lookup = (double)ic / keys.size();
  r.ns_per_lookup =
      std::chrono::duration<double, std::nano>(l1 - l0).count() / keys.size();

  balance_stats(before, n, r.peak_to_mean, r.min_to_mean, r.cv);

  int trials = std::min(n, 8);
  double dsum = 0, isum = 0, usum = 0;
  for (int t = 0; t < trials; t++) {
    int victim = (int)((uint64_t)t * n / trials);
    auto u0 = std::chrono::steady_clock::now();
    conhash_del_node(ch, &nodes[victim]);     // in-place removal of its vnodes
    auto u1 = std::chrono::steady_clock::now();
    usum += std::chrono::duration<double, std::micro>(u1 - u0).count();

    std::vector<int> after(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%llu", (unsigned long long)keys[i]);
      const struct node_s* nd = conhash_lookup(ch, buf);
      after[i] = nd ? (int)(intptr_t)nd->data - 1 : -1;
    }
    double ideal = 0;
    dsum += disruption_of(before, after, victim, ideal);
    isum += ideal;
    conhash_add_node(ch, &nodes[victim]);     // restore
  }
  r.disruption = dsum / trials;
  r.ideal = isum / trials;
  r.excess = r.disruption - r.ideal;
  r.update_us = usum / trials;
  conhash_fini(ch, node_fini_noop);
  return r;
}

// ---------------------------------------------------------------------- main
int main(int argc, char** argv) {
  size_t K = (argc > 1) ? strtoul(argv[1], nullptr, 10) : 1000000;

  std::vector<uint64_t> keys(K);
  for (size_t i = 0; i < K; i++) keys[i] = splitmix64(KEY_SEED + i);

  {
    Counter probe(PERF_COUNT_HW_CPU_CYCLES);
    fprintf(stderr, "keys=%zu  pmu=%s\n", K,
            probe.ok() ? "available" : "UNAVAILABLE");
  }

  const int backend_counts[] = {8, 16, 32, 64, 128};
  // katran default 65537; cilium maglev default 16381; dpvs mh default 4093
  const uint32_t ring_sizes[] = {65537, 16381, 4093};

  std::vector<Row> rows;
  for (int n : backend_counts) {
    for (uint32_t rs : ring_sizes) {
      rows.push_back(run_maglev(katran::HashFunction::Maglev,
                                "katran-maglev", n, rs, false, keys));
      rows.push_back(run_maglev(katran::HashFunction::MaglevV2,
                                "katran-maglev-v2", n, rs, false, keys));
    }
    // Weighted case at katran's own ring size only: this is the configuration
    // MaglevHashV2 exists for.
    rows.push_back(run_maglev(katran::HashFunction::Maglev,
                              "katran-maglev", n, 65537, true, keys));
    rows.push_back(run_maglev(katran::HashFunction::MaglevV2,
                              "katran-maglev-v2", n, 65537, true, keys));

    rows.push_back(run_anchorhash(n, keys));
    rows.push_back(run_conhash(n, false, keys));
    rows.push_back(run_conhash(n, true, keys));
    fprintf(stderr, "  done n=%d\n", n);
  }

  printf("impl,weights,backends,table,disruption,ideal,excess,peak_to_mean,"
         "min_to_mean,cv,build_us,update_us,cycles_per_lookup,ins_per_lookup,"
         "ns_per_lookup\n");
  for (const auto& r : rows) {
    printf("%s,%s,%d,%u,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%.1f,%.1f,%.2f,%.2f,%.2f\n",
           r.impl.c_str(), r.weights.c_str(), r.backends, r.table, r.disruption,
           r.ideal, r.excess, r.peak_to_mean, r.min_to_mean, r.cv, r.build_us,
           r.update_us, r.cycles_per_lookup, r.ins_per_lookup, r.ns_per_lookup);
  }
  return 0;
}
