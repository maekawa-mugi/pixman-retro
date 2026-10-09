> On the unified `mmi` branch, sources and harness upgrades are already integrated. Run `bash test/ps2/verify-host.sh` and `bash test/ps2/build-from-clone.sh` from the repository root; ZIP staging is unnecessary. The staging instructions below describe the original audit workflow.

# EE latency variants: ZIP v5 audit workspace

The uploaded ZIP's complete v5 integration lives in
`build-ps2-mmi/all-pr/source/` **not** in the repository root.
This branch starts from `feat/ps2-ee-mmi-initial-20261009`; it
intentionally does **not** pretend that the uncommitted v5 integration
has already been pushed. Apply the separate v5 review patch to the
local integrated source before rebuilding the PS2 ELF.

## Candidate kernels, not asserted winners

* `nearest_opaque`: original baseline (1 quad/iteration).
* `nearest_opaque_u2`: 2 quad loads before using loaded registers.
* `nearest_opaque_u4`: 4 quad loads, 4 independent PORS and stores.
* `bilinear_hsum`: original PMULTH/PMFHL pipeline.
* `bilinear_hsum_sched`: all 4 coefficient vectors loaded first,
  and independent PEXTUB operations placed between a PMULTH and the
  PMFHL.LH that consumes its HI/LO result.
* `bilinear_hsum_predecode`: decode low and high byte lanes for all
  four pixels before multiplying, with a different register-pressure /
  dependency tradeoff. This is the third HSum alternative.

The EE Core User Manual says integer load results are interlocked,
multimedia multiply has ~4 cycle result latency and throughput 2,
and exact dual-issue/caches prevent reliable static cycle estimates.
Candidate variants are conditional experiments, not measured speedups.

All entry points require 16-byte alignment and nonoverlapping buffers.
They are kept separate from Pixman dispatch until real PS2 correctness
and timing prove a win. n32 passes the 5th argument in $8, o32
on the stack at 16($sp). Only volatile physical registers are clobbered.

Build the local v5 harness and compare **independent** A/B/C results
with the same count, aligned input bytes and unchanged source and
canaries. Repeated trials are needed; cold-cache, interrupted timer,
and warmed data can invert apparent rankings.

## Findings in ZIP harness

* A group can return failure while the 17-line dashboard still marks
  both sides green if no earlier per-case mismatch was recorded. The
  group failure must flag the per-row status explicitly.
* The single-run API benchmark measures only one composite per side;
  that is adequate for correctness but noisy for measuring latency.
* MMI call coverage is checked only at the very end. The result should
  include the missing symbol name and variant, not just an index.
* The host scalar model never runs a real PMULTH; native PASS is
  a harness check, **not** evidence of EE correctness.

See the accompanying patch for v5's PS2SDK build wiring, stronger
diagnostics, and the A/B/C experiment runner.

## Non-scaling kernels

See `README-throughput.md` for 2x/8x ADD/COPY/FILL, 2x OVER,
predecoded A8 IN and 2x 8888→RGB565 SRC variants. These are measured
separately, never installed in the production dispatch by default.
