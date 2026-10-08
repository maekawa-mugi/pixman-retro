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

The suite combines the pre-existing component-alpha, solid+A8, RGB565, and image+A8
differential tests, adds scalar-vs-MMI tests of ADD, COPY, FILL, A8 IN,
unmasked 8888 OVER, and 8888-to-565 SRC. All aligned assembly tests
include zero counts and untouched canaries. The timing section includes fused Solid+CA, Solid+A8, image+A8, RGB565 OVER and 8888->565 SRC and emits
`BENCH_CSV` records (microseconds and C/MMI speedup) for ten operations
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

The fused image+A8 case is compiled as a fourth independent differential
test inside the same one-shot suite. It targets the actual o32 fifth
argument and all 256 coverage values.

## Native preflight (no PS2 needed)

`make -f test/Makefile.ee-mmi host-check` compiles every current suite
test against a scalar reference backend and executes all differential
test binaries in one host executable. This catches C syntax, linking,
test-output, and test harness regressions before transferring to PS2.
It does **not** assemble EE instructions, validate MMI hardware results,
confirm the o32 fifth-argument location, or establish speedups.

A dedicated GitHub Actions workflow performs the same host preflight
for branch pushes. The EE cross-compile and actual one-shot target run
remain mandatory before deploying MMI changes.

## Pixman API dispatch benchmark

When `-Dee-mmi=enabled`, Meson also builds the
`ee-mmi-pixman-bench` executable in `test/`. Include that binary in the
directory passed as the second argument to `run-ee-mmi-all.sh`.
The runner executes it twice, once with EE MMI enabled and once with
`PIXMAN_DISABLE=ee-mmi`. The `PIXMAN_BENCH` CSV lines cover 14 Pixman
API workloads, including masked OVER, component-alpha, RGB565, nearest
and bilinear scaling, ADD, SRC, IN, BLT and fill, at 16x2, 64x16,
and 256x64. Compare the checksum field between enabled/disabled modes
for each workload/size before comparing timings. Different checksums
are a correctness failure and must be investigated, independent of
the benchmark speed.

The microbench suite and Pixman API bench are complementary. Only
target hardware can establish speedups and the two processes must be
run under the same load and cache policy.

## Single transfer, single run

After cross-building `ee-mmi-suite` and the Meson Pixman test binaries
with the same EE Linux compiler and ABI:

```sh
sh test/package-ee-mmi.sh build-ee/test ee-mmi-test-kit.tar.gz
```

Copy only the generated archive to PS2 Linux, unpack it, and follow
`README-RUN.txt`. Retrieve `ee-mmi-results.log` once. The runner
collects failures without stopping and compares all 42 Pixman API
benchmark checksums across MMI-enabled and generic-C runs, reporting
`EE_PIXEL_MISMATCH` on differences. The log includes platform info,
all unit-test failures, all microbench CSV lines and all Pixman API
bench CSV lines.

The archive does **not** include transitive shared libraries. If
Pixman was built dynamically, make sure the matching libpixman is
installed or package it separately; a single archive does not itself
guarantee that the binaries are self-contained.

The GitHub Actions host preflight can fail for runner provisioning or
other platform reasons; without job logs and completed build steps
it must not be interpreted as evidence that the EE code compiled or
failed. Run `host-check` locally before archiving.

## Exhaustive Pixman test mode

Set `EE_FULL=1` before running the kit to additionally execute
`pixel-test`, `fetch-test`, `solid-test`, `cover-test`,
`scaling-helpers-test`, `affine-test`, `composite-traps-test`,
and `glyph-test` with EE MMI enabled **and** disabled. The packager
includes these if they exist in the Meson test build. Missing tests
are marked as failures in full mode. Full mode can be slow on the
R5900 but avoids another round-trip to investigate obscure pixels.
