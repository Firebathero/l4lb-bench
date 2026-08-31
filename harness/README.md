# harness

## Layout

```
harness/
  gen/        traffic generation configs   (empty, unimplemented)
  run/        experiment drivers           ch_bench.cpp, see run/README.md
  collect/    perf/PMU collection          pmu_probe.py, pmu_linearity.py
  viz/        plotting                     (empty, unimplemented)
  results/    raw output                   (gitignored)
```

## What works today

**`run/ch_bench.cpp`** measures four real consistent-hashing implementations
compiled from `repos/`: katran `MaglevHash` and `MaglevHashV2`, AnchorHash, and
dpvs `libconhash`. See `run/README.md` for the method and the two disclosed
build deviations.

```bash
cd run && make && make run     # writes ../results/ch_bench.csv
```

Committed results: `../notes/data/ch_bench.csv`.
Analysis: `../notes/consistent-hashing.md`.

**`collect/pmu_probe.py`** and **`collect/pmu_linearity.py`** read hardware
performance counters through `perf_event_open(2)` via ctypes. No compiler, no
`perf` binary, no packages. Used to establish that the counters on this host are
proportional and repeatable before anything was measured with them.

```bash
python3 collect/pmu_probe.py       # do the counters count?
python3 collect/pmu_linearity.py   # are they proportional and repeatable?
```

Findings: `../notes/pmu-validation.md`.

## What does not work yet

`gen/` and `viz/` are empty. No packet-level experiment exists, because the host
in `../env/host.md` cannot run one: no hugepages, no IOMMU groups, no NIC
bindable to a userspace driver, no generator/DUT separation. That work needs
different hardware.
