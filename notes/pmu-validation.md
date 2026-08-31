# PMU validation

`env/host.md` recorded that the `cpu` PMU *declares* eight hardware events, and
flagged that whether Hyper-V virtualises the underlying counters through to the
guest could not be settled from sysfs alone. This settles it.

Method: `perf_event_open(2)` called directly through `ctypes`, counting a
workload of known, controllable size. No packages installed, no compiler, no
`perf` binary. Script: `harness/collect/pmu_probe.py` and
`harness/collect/pmu_linearity.py`.

Date: 2026-08-30. Host as described in `env/host.md`.

## Do the counters count?

`perf_event_paranoid = 2`, so events were opened with `exclude_kernel=1` and
`exclude_hv=1`, counting this process's user-space only.

| event | opened | delta over the workload |
|---|---|---|
| cpu-cycles | yes | 1,172,299,066 |
| instructions | yes | 5,895,079,009 |
| cache-references | yes | 345,320 |
| cache-misses | yes | 23,639 |
| branch-instructions | yes | 1,076,039,089 |
| branch-misses | yes | 35,397 |
| stalled-cycles-frontend | yes | 9,445,586 |

All seven opened and returned non-zero.

## Are they proportional to work?

Non-zero is not sufficient for benchmarking. A fixed workload was scaled 1x, 2x
and 4x, using bounded 64-bit arithmetic so the work per iteration is constant.

| iterations | instructions | ins/iter | cycles | cyc/iter |
|---|---|---|---|---|
| 2,000,000 | 3,186,719,847 | 1593.4 | 685,043,078 | 342.5 |
| 4,000,000 | 6,373,530,018 | 1593.4 | 1,352,821,307 | 338.2 |
| 8,000,000 | 12,747,032,334 | 1593.4 | 2,742,452,130 | 342.8 |

Instructions per iteration is identical to four significant figures at every
size. Scaling error against the 1x baseline: **0.0%** at 2x and 4x.

## Are they repeatable?

Seven runs of the same 2,000,000-iteration workload:

```
3,212,717,159   3,212,717,116   3,212,717,059   3,212,717,092
3,212,717,068   3,212,717,217   3,212,717,122
```

mean 3,212,717,119, stdev **51**, relative stdev **0.000%**.

## Verdict

Cycle and instruction counting on this host is proportional and repeatable, and
is usable for relative measurement. This is what makes the algorithm-level work
in `consistent-hashing.md` possible on a machine that cannot run any of the
packet datapaths.

## What is still not available

From `env/host.md`, unchanged by this result:

- `caps/max_precise = 0`, so no precise-event sampling (no IBS-backed `:p`)
- no TLB event aliases, no `stalled-cycles-backend`
- no uncore, L3 or data-fabric PMUs (`amd_df`, `amd_l3`, `amd_umc` absent)
- `perf` itself is not installed, and `linux-tools` for
  `6.18.33.2-microsoft-standard-WSL2` is not a package in the archive

So: counting works, sampling and uncore analysis do not.

## Correction to an earlier reading

The first probe reported an IPC of 5.03 and that was flagged as possibly too
high to trust. The controlled workload above shows exact proportionality, so the
counters are genuinely counting. The high IPC is a property of the workload,
which is a tight, branch-predictable integer loop on a 6-wide core, not evidence
of a synthetic counter.
