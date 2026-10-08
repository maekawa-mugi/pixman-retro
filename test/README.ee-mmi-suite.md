# EE MMI one-shot test and benchmark package

This target is for **PS2 Linux / R5900 / o32**, not PS2SDK or Loongson MMI.

From the repository root (replace with the configured compiler and CPU
flags actually used by the Pixman build):

```sh
make -f test/Makefile.ee-mmi EE_CC="$EE_CC" EE_CFLAGS="-O2 <your EE flags>"
```

Transfer **one binary**, `ee-mmi-suite`, to PS2 Linux. Run once and save
stdout/stderr, including the exit status:

```sh
./ee-mmi-suite > ee-mmi-results.txt 2>&1
echo "EXIT_CODE=$?" >> ee-mmi-results.txt
```

The suite combines the pre-existing component-alpha, solid+A8, and RGB565
differential tests, adds scalar-vs-MMI tests of ADD, COPY, FILL, A8 IN,
unmasked 8888 OVER, and 8888-to-565 SRC. All aligned assembly tests
include zero counts and untouched canaries. The timing section emits
`BENCH_CSV` records (microseconds and C/MMI speedup) for five operations
at 64, 256, 1024, 4096, and 16384 bytes.

To collect the **whole Pixman** dispatch/correctness checks too, transfer
the existing Pixman test binaries and run:

```sh
sh test/run-ee-mmi-all.sh ./ee-mmi-suite ./pixman-test-bin > ee-all.log 2>&1
echo "EXIT_CODE=$?" >> ee-all.log
```

The runner executes each available test both with EE MMI enabled and with
`PIXMAN_DISABLE=ee-mmi`. Missing required test binaries are failures.
Use `./ee-mmi-suite --test-only` or `--bench-only` if needed.

Interpretation: these microbenchmarks measure hot, aligned kernels only,
not full Pixman API dispatch, cold-cache cases or full-screen workload
performance. `gettimeofday()` has limited precision; values below one
microsecond are `N/A`. No test is assumed to pass before running the
actual EE toolchain and PS2 Linux binary. A performance regression does
not mean a correctness failure; compare independent kernel timings
rather than aggregating incompatible workloads.

**Capture in the same visit:** compiler/binutils versions; full logs;
backend enabled/disabled; test exit status; elapsed microseconds; CPU
clock/emulator version; 8/16/32bpp and 565 alignment/repeat/negative-stride
results. The Meson Pixman tests are needed to cover routing and clipping
beyond direct assembly entry points.
