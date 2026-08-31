#!/usr/bin/env python3
"""Are the counters proportional to work, and repeatable?

Nonzero counters are not enough to benchmark with. This scales a fixed
workload 1x/2x/4x and checks the counts scale with it, then repeats the
same size several times to measure run-to-run spread.
"""
import ctypes, ctypes.util, os, struct, statistics

SYS_perf_event_open = 298
libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
libc.syscall.restype = ctypes.c_int


def attr(config):
    flags = (1 << 5) | (1 << 6)          # exclude_kernel, exclude_hv
    return struct.pack("=IIQQQQQIIQQQQiiQIHHIIQQ",
                       0, 136, config, 0, 0, 0, flags, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0)


def measure(config, iters):
    buf = ctypes.create_string_buffer(attr(config), 136)
    fd = libc.syscall(SYS_perf_event_open, ctypes.byref(buf),
                      ctypes.c_int(0), ctypes.c_int(-1),
                      ctypes.c_int(-1), ctypes.c_ulong(0))
    if fd < 0:
        raise OSError(os.strerror(ctypes.get_errno()))
    before = struct.unpack("=Q", os.read(fd, 8))[0]
    # bounded 64-bit arithmetic: constant work per iteration, no bignum growth
    x = 12345
    m = (1 << 64) - 1
    for i in range(iters):
        x = ((x * 6364136223846793005) + 1442695040888963407) & m
    after = struct.unpack("=Q", os.read(fd, 8))[0]
    os.close(fd)
    return after - before


INS, CYC = 1, 0
BASE = 2_000_000

print("=== linearity: does the count scale with the work? ===")
print(f"{'iters':>12} {'instructions':>18} {'ins/iter':>10} {'cycles':>16} {'cyc/iter':>10}")
base_ins = None
for mult in (1, 2, 4):
    n = BASE * mult
    ins = measure(INS, n)
    cyc = measure(CYC, n)
    print(f"{n:>12,} {ins:>18,} {ins/n:>10.1f} {cyc:>16,} {cyc/n:>10.1f}")
    if mult == 1:
        base_ins = ins
    else:
        ratio = ins / base_ins
        print(f"{'':>12} scaling vs 1x: {ratio:.3f} (ideal {float(mult):.3f}, error {abs(ratio-mult)/mult*100:.1f}%)")

print()
print("=== repeatability: same workload, 7 runs ===")
runs = [measure(INS, BASE) for _ in range(7)]
mean = statistics.mean(runs)
sd = statistics.pstdev(runs)
for i, r in enumerate(runs):
    print(f"  run {i+1}: {r:>15,}  ({(r-mean)/mean*100:+.2f}%)")
print(f"  mean {mean:,.0f}   stdev {sd:,.0f}   relative stdev {sd/mean*100:.3f}%")

print()
print("=== interpretation ===")
err = abs(measure(INS, BASE*4) / base_ins - 4) / 4 * 100
print(f"  4x scaling error : {err:.1f}%")
print(f"  run-to-run spread: {sd/mean*100:.3f}%")
if err < 5 and sd / mean < 0.05:
    print("  Counters are proportional and repeatable. Usable for relative measurement.")
else:
    print("  Counters are not reliably proportional or repeatable. Do not benchmark on them.")
