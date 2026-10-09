> On the unified `mmi` branch, sources and harness upgrades are already integrated. Run `bash test/ps2/verify-host.sh` and `bash test/ps2/build-from-clone.sh` from the repository root; ZIP staging is unnecessary. The staging instructions below describe the original audit workflow.

# Imported PS2 v5 harness: measured latency alternatives

The uploaded `pixman-retro.zip` contains the full v5 integration under
`build-ps2-mmi/all-pr/source/`. That directory is ignored by the root
repository's Git configuration. The review branch holds the **new
experimental sources and source upgrader**, not the original 65-MB ZIP,
a prebuilt ELF, or an asserted complete v5 merge.

## Stage once from Windows PowerShell

From `C:\Users\mugi\Documents\GitHub\pixman-retro`:

```powershell
git fetch origin codex/ps2-v5-audit-latency-20261009
git restore --source=origin/codex/ps2-v5-audit-latency-20261009 -- test/ps2/latency-variants-asm.S test/ps2/latency-variants-test.c test/ps2/latency-variants-host.c test/ps2/upgrade-v5-harness.py test/ps2/stage-v5-latency.ps1 test/ps2/throughput-core-asm.S test/ps2/throughput-blend-asm.S test/ps2/throughput-variants-test.c test/ps2/throughput-variants-host.c test/ps2/upgrade-v5-throughput.py
powershell -NoProfile -ExecutionPolicy Bypass -File test/ps2/stage-v5-latency.ps1
```

The staging helper copies **nine review files** into the existing
v5 integrated source and applies two idempotent upgrades to v5
`progress.c`, `main.c`, `build.sh` and `verify-host.sh`. No file
in the ZIP's `.git`, no ELF, and no production MMI kernel is overwritten.
To inspect the exact edits, see the standalone `ps2-v5-harness-audit.patch`
review artifact.

## Run before visiting PS2

In MSYS2/WSL Bash with Meson, Ninja, and the appropriate compiler:

```sh
cd build-ps2-mmi/all-pr/source
bash test/ps2/verify-host.sh
bash test/ps2/build.sh
```

The host script performs the original integrated v5 tests plus
normal nearest/HSUM candidate tests and two negative controls:
`PS2_LATENCY_TEST_FAULT` must fail the nearest u4 test, while
`PS2_PROGRESS_FORCE_GROUP_FAILURE` must show red X in the previously
false-green dashboard row and fail the suite. If the host build fails,
do not send the newly linked ELF to PS2 yet.

On target, every candidate is compared with a scalar reference before
latency CSV lines are emitted. The old nearest u1, new u2 and u4,
the original, scheduled and predecoded bilinear HSUM alternatives
are tested for 0..64 quads,
input immutability and buffer canaries. Timings cover 64, 256,
1024 bytes of nearest input, 192 repeated calls at each size, and
corresponding HSum sizes (3 variants each). Same-size candidates use **identical seeded
input data**. The MMIs are experiments and deliberately do **not**
replace the main Pixman dispatch path.

## Harness audit observations

The full v5 screen has 17 rows of A/B results. The original
`ps2_progress_group()` could report a group-level error with no
per-pixel mismatch and still paint both A/B as green. The upgrader
marks both X when side attribution is unknown and prints
`GROUP_UNATTRIBUTED_FAILURE`. A host negative-control injection
tests that behavior, rather than relying only on a passive inspection.

The existing v5 API benchmark does only one composite per mode/case,
which can verify correctness but is too noisy to establish cycle wins.
The separate latency candidate bench repeats each kernel 192 times;
even then, rankings are indicative until measured on PS2 hardware.
The host scalar models do **not** validate PMULTH timing or R5900 ABI.

Manual: EE Core User Manual section 2.12 lists 4-cycle multimedia
multiply latency, throughput 2, and cache-hit loads of 1 cycle; data
hazards interlock and dual issue/cache behavior can change outcomes.

## Non-scaling MMI A/B/C throughput variants

This branch also tests the other major EE scanline primitives:

| Family | A baseline | B alternative | C alternative |
|---|---|---|---|
| ADD / COPY / FILL | current u4 | u2 | u8 |
| ARGB8888 OVER | current u1 | u2 | none |
| A8 IN | current u1 | early upper-byte predecode | none |
| ARGB8888 → RGB565 SRC | current u1 | u2 | none |
| ARGB8888 → RGB565 fused OVER | current u1 | u2 | none |

Ten experimental implementations are provided in
`throughput-core-asm.S` and `throughput-blend-asm.S`. The
`ps2_throughput_run()` regression checks all 17 baseline/candidate
kernels at all 65 block counts (0..64), giving **1105 comparisons**,
including full-buffer canaries, a scalar oracle and unchanged source.
An invalid candidate prevents benchmark output.

`THROUGHPUT_CSV` provides **357 measurements** (15 kernels x seven
sizes: 16, 32, 64, 128, 256, 512 and 1024 destination bytes) with 256
repeated calls in three independent trials each (rotated candidate order) and identical seeded inputs per size/family.
`PS2_THROUGHPUT_TEST_FAULT` deliberately corrupts one ADD-u8 pixel;
the host harness must reject it.

```sh
make -f test/Makefile.ee-mmi throughput-host-check
# In the staged ZIP v5 source with PS2DEV/PS2SDK:
bash test/ps2/verify-host.sh
bash test/ps2/build.sh
```

The staging PowerShell helper imports the nine source/upgrader files
into `build-ps2-mmi/all-pr/source/test/ps2/`. Neither the source
library's kernel dispatch nor the shared feature branch is replaced.

**Real EE assembler validation, PS2SDK linking, hardware execution and
speed rankings are still outstanding.** Large unrolls can increase
I-cache pressure; the manual's dependency latency alone cannot predict
the winner.
