#!/usr/bin/env python3
"""Does the hardware PMU actually count through Hyper-V?

Opens perf events directly via perf_event_open(2) using ctypes. Installs
nothing, needs no compiler, needs no perf binary. Counts a loop of known
size and checks whether the returned numbers are real or stubbed to zero.
"""
import ctypes, ctypes.util, os, struct, sys

SYS_perf_event_open = 298  # x86_64

PERF_TYPE_HARDWARE = 0
HW = {
    "cpu-cycles":           0,
    "instructions":         1,
    "cache-references":     2,
    "cache-misses":         3,
    "branch-instructions":  4,
    "branch-misses":        5,
    "stalled-cycles-front": 7,
}

libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
libc.syscall.restype = ctypes.c_int


def attr(config, exclude_kernel=True):
    # perf_event_attr, PERF_ATTR_SIZE_VER8 == 136 bytes
    flags = 0
    if exclude_kernel:
        flags |= (1 << 5)   # exclude_kernel
    flags |= (1 << 6)       # exclude_hv
    return struct.pack(
        "=IIQQQQQIIQQQQiiQIHHIIQQ",
        PERF_TYPE_HARDWARE,  # type
        136,                 # size
        config,              # config
        0,                   # sample_period
        0,                   # sample_type
        0,                   # read_format
        flags,               # bitfield flags
        0,                   # wakeup_events
        0,                   # bp_type
        0, 0,                # config1, config2
        0,                   # branch_sample_type
        0,                   # sample_regs_user
        0,                   # sample_stack_user
        0,                   # clockid
        0,                   # sample_regs_intr
        0,                   # aux_watermark
        0, 0,                # sample_max_stack, __reserved_2
        0, 0,                # aux_sample_size, __reserved_3
        0,                   # sig_data
        0,                   # config3
    )


def open_counter(config):
    buf = ctypes.create_string_buffer(attr(config), 136)
    ctypes.set_errno(0)
    fd = libc.syscall(SYS_perf_event_open, ctypes.byref(buf),
                      ctypes.c_int(0),    # pid: this process
                      ctypes.c_int(-1),   # cpu: any
                      ctypes.c_int(-1),   # group_fd
                      ctypes.c_ulong(0))
    if fd < 0:
        return None, os.strerror(ctypes.get_errno())
    return fd, None


def read_counter(fd):
    return struct.unpack("=Q", os.read(fd, 8))[0]


ITERS = 3_000_000

print(f"perf_event_paranoid = {open('/proc/sys/kernel/perf_event_paranoid').read().strip()}")
print(f"loop iterations     = {ITERS:,}")
print()
print(f"{'event':<22} {'opened':<8} {'delta count':>18}   verdict")
print("-" * 72)

results = {}
for name, cfg in HW.items():
    fd, err = open_counter(cfg)
    if fd is None:
        print(f"{name:<22} {'NO':<8} {'-':>18}   perf_event_open failed: {err}")
        results[name] = None
        continue
    before = read_counter(fd)
    x = 0
    for i in range(ITERS):          # deliberately branchy integer work
        x = (x + i) ^ (x >> 3)
    after = read_counter(fd)
    os.close(fd)
    delta = after - before
    results[name] = delta
    if delta == 0:
        verdict = "ZERO -> not counting"
    elif delta < ITERS // 100:
        verdict = "implausibly low"
    else:
        verdict = "plausible"
    print(f"{name:<22} {'yes':<8} {delta:>18,}   {verdict}")

print()
ins = results.get("instructions")
cyc = results.get("cpu-cycles")
if ins and cyc:
    print(f"instructions / loop iteration = {ins / ITERS:6.1f}")
    print(f"IPC (instructions / cycles)   = {ins / cyc:6.2f}")
    print()
    if 0.05 < ins / cyc < 8.0 and ins / ITERS > 3:
        print("VERDICT: hardware counters are being virtualised through to this guest.")
    else:
        print("VERDICT: numbers are not physically plausible. Treat the PMU as unusable.")
else:
    print("VERDICT: core events unavailable. Hardware PMU is NOT usable in this guest.")
