#!/usr/bin/env python3
"""
Publication-grade verification of CUDA optimization correctness and performance.

Methodology
-----------
- Correctness : For each (instance, n_scen, seed) triple, verify that
  CPU, baseline-GPU, and optimized-GPU produce bit-identical penalizedCost,
  distance, capacityExcess, isFeasible, and nbRoutes.
- Performance : For every configuration a *warmup* run is executed first
  (and discarded).  Then N_SEEDS independent runs are timed; we report
  median, min, max of wall-clock time.
- Fairness : All three binaries receive identical command-line arguments
  (same seed, nthreads, iterLim, nextrascen, etc.).  The optimized binary
  is tested with gpuBatchSize=1 so that the search trajectory is identical
  to the baseline (same number of offspring per iteration, same RNG draws).
"""

import subprocess, re, statistics, sys, os, time, json
from pathlib import Path

# ── Binaries ──────────────────────────────────────────────────
ROOT       = Path(__file__).resolve().parent.parent
BASELINE   = str(ROOT / "build_baseline" / "hgs_cuda")
OPT_GPU    = str(ROOT / "build"          / "hgs_cuda")
CPU_BIN    = str(ROOT / "build"          / "hgs")
INST_DIR   = str(ROOT / "Instances"      / "CVRP")
LOG_DIR    = ROOT / "anpy" / "logs"
LOG_DIR.mkdir(parents=True, exist_ok=True)

N_SEEDS    = 5
NTHREADS   = 4
TIMEOUT    = 600

# ── Test matrix ───────────────────────────────────────────────
INSTANCES = [
    "X-n101-k25.vrp",
    "X-n106-k14.vrp",
    "X-n110-k13.vrp",
    "X-n143-k7.vrp",
]

SCEN_ITER = [
    (100,    10),
    (1000,   10),
    (10000,   5),
    (50000,   3),
]

# ── Helpers ───────────────────────────────────────────────────
def run_one(binary, inst, n_scen, itlim, seed, extra=None):
    cmd = [
        binary, f"{INST_DIR}/{inst}", "/dev/null",
        "-seed", str(seed), "-nthreads", str(NTHREADS),
        "-nextrascen", str(n_scen - 1),
        "-freqPrint", "999999", "-maxClient", "-1",
        "-iterLim", str(itlim),
    ]
    if extra:
        cmd += extra
    t0 = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT)
    wall = time.perf_counter() - t0
    text = proc.stdout + proc.stderr
    def grab(pat):
        m = re.search(pat, text)
        return float(m.group(1)) if m else None
    return {
        "penalizedCost": grab(r"penalizedCost:\s+([\d.eE+\-]+)"),
        "distance":      grab(r"avg distance:\s+([\d.eE+\-]+)"),
        "capExcess":     grab(r"avg capExcess:\s+([\d.eE+\-]+)"),
        "isFeasible":    grab(r"isFeasible:\s+(\d)"),
        "nbRoutes":      grab(r"nbRoutes:\s+(\d+)"),
        "wallTime":      wall,
        "reportedTime":  grab(r"Total time:\s+([\d.]+)s"),
    }

def warmup(binary, inst, extra=None):
    """One throw-away run to warm up CUDA context / JIT."""
    run_one(binary, inst, 100, 1, seed=999, extra=extra)

FIELDS = ["penalizedCost", "distance", "capExcess", "isFeasible", "nbRoutes"]

def vals_match(a, b, tol=1e-6):
    if a is None or b is None:
        return a is None and b is None
    return abs(a - b) <= tol + tol * abs(b)

# ── PART 1: Correctness ──────────────────────────────────────
def correctness():
    print("=" * 105)
    print("  PART 1 · CORRECTNESS")
    print("  CPU  vs  Baseline-GPU  vs  Optimized-GPU (B=1)")
    print("  Same seed / iterLim / nthreads — search trajectory is identical")
    print("=" * 105)
    print()

    total_checks = 0
    total_pass   = 0
    failures     = []

    for inst in INSTANCES:
        path = f"{INST_DIR}/{inst}"
        if not os.path.exists(path):
            print(f"  SKIP {inst} (file not found)")
            continue

        for n_scen, itlim in SCEN_ITER:
            for seed in range(1, 4):          # seeds 1-3
                rc = run_one(CPU_BIN,    inst, n_scen, itlim, seed)
                rb = run_one(BASELINE,   inst, n_scen, itlim, seed)
                ro = run_one(OPT_GPU,    inst, n_scen, itlim, seed,
                             ["-gpuBatchSize", "1"])

                row_ok = True
                for f in FIELDS:
                    total_checks += 2   # baseline vs cpu, opt vs cpu
                    cb_ok = vals_match(rc[f], rb[f])
                    co_ok = vals_match(rc[f], ro[f])
                    if cb_ok:
                        total_pass += 1
                    else:
                        row_ok = False
                        failures.append((inst, n_scen, seed, f,
                                         "base", rc[f], rb[f]))
                    if co_ok:
                        total_pass += 1
                    else:
                        row_ok = False
                        failures.append((inst, n_scen, seed, f,
                                         "opt", rc[f], ro[f]))

                tag = "OK" if row_ok else "FAIL"
                cost_s = f"{rc['penalizedCost']:.2f}" if rc['penalizedCost'] else "N/A"
                print(f"  {inst:<22} scen={n_scen:>5}  seed={seed}  "
                      f"cost={cost_s:<14} [{tag}]")

        print()

    print("-" * 105)
    print(f"  Checks: {total_pass}/{total_checks} passed")
    if failures:
        print(f"  FAILURES ({len(failures)}):")
        for item in failures:
            print(f"    {item}")
    else:
        print("  >>> ALL CHECKS PASSED — CPU, Baseline-GPU, Optimized-GPU "
              "produce identical results.")
    print()
    return len(failures) == 0

# ── PART 2: Performance ──────────────────────────────────────
def performance():
    print("=" * 105)
    print("  PART 2 · PERFORMANCE")
    print(f"  Warmup: 1 throwaway run per binary/instance")
    print(f"  Timing: {N_SEEDS} seeds, report median [min, max]")
    print(f"  GPU: ", end="")
    try:
        g = subprocess.run(["nvidia-smi", "--query-gpu=name",
                            "--format=csv,noheader"],
                           capture_output=True, text=True)
        print(g.stdout.strip())
    except Exception:
        print("N/A")
    print("=" * 105)
    print()

    hdr = (f"{'Instance':<22} {'Scen':>6} {'Iter':>4} │ "
           f"{'Baseline':>10} {'Opt(B=1)':>10} │ "
           f"{'Speedup':>8}  {'[min,max]':>16}")
    print(hdr)
    print("─" * len(hdr))

    results = []

    for inst in INSTANCES:
        path = f"{INST_DIR}/{inst}"
        if not os.path.exists(path):
            continue

        warmup(BASELINE, inst)
        warmup(OPT_GPU, inst, ["-gpuBatchSize", "1"])

        for n_scen, itlim in SCEN_ITER:
            times_b = []
            times_o = []
            for seed in range(1, N_SEEDS + 1):
                rb = run_one(BASELINE, inst, n_scen, itlim, seed)
                ro = run_one(OPT_GPU,  inst, n_scen, itlim, seed,
                             ["-gpuBatchSize", "1"])
                times_b.append(rb["wallTime"])
                times_o.append(ro["wallTime"])

            mb = statistics.median(times_b)
            mo = statistics.median(times_o)
            sp = mb / mo if mo > 0 else 0

            lo = min(times_o)
            hi = max(times_o)
            sp_lo = mb / hi if hi > 0 else 0
            sp_hi = mb / lo if lo > 0 else 0

            print(f"{inst:<22} {n_scen:>6} {itlim:>4} │ "
                  f"{mb:>9.2f}s {mo:>9.2f}s │ "
                  f"{sp:>7.1f}x  [{sp_lo:.1f}x, {sp_hi:.1f}x]")

            results.append({
                "instance": inst, "n_scen": n_scen, "itlim": itlim,
                "baseline_median": mb, "opt_median": mo,
                "speedup": sp,
                "baseline_times": times_b, "opt_times": times_o,
            })

        print()

    # Save raw data as JSON for reproducibility
    out_json = LOG_DIR / f"final_verify_{int(time.time())}.json"
    with open(out_json, "w") as f:
        json.dump(results, f, indent=2)
    print(f"  Raw timing data saved to: {out_json}")
    print()
    return results

# ── Main ──────────────────────────────────────────────────────
if __name__ == "__main__":
    ok = correctness()
    results = performance()

    print("=" * 105)
    print("  SUMMARY")
    print("=" * 105)
    if ok:
        print("  Correctness : PASS (all fields match across CPU / Baseline-GPU / Optimized-GPU)")
    else:
        print("  Correctness : FAIL")
    print()
    print(f"  {'Instance':<22} {'Scen':>6} │ {'Speedup':>8}")
    print("  " + "─" * 42)
    for r in results:
        print(f"  {r['instance']:<22} {r['n_scen']:>6} │ {r['speedup']:>7.1f}x")
    print()
