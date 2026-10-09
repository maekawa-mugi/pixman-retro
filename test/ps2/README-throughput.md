> On the unified `mmi` branch, sources and harness upgrades are already integrated. Run `bash test/ps2/verify-host.sh` and `bash test/ps2/build-from-clone.sh` from the repository root; ZIP staging is unnecessary. The staging instructions below describe the original audit workflow.

# PS2 R5900 non-scaling MMI throughput experiments

Use `stage-v5-latency.ps1` to stage nine review files into the ZIP
v5 integrated source, then `bash test/ps2/verify-host.sh` and
`bash test/ps2/build.sh` with the PS2 toolchain.

The seven operation families are ADD, COPY, FILL, ARGB8888 OVER, A8 IN,
ARGB8888-to-RGB565 SRC and fused ARGB8888-to-RGB565 OVER. The three memory primitives compare current
4-quad, new 2-quad and new 8-quad processing. Others compare current
versions against unrolled-2 or predecoded upper byte lanes.

**Acceptance criteria:** C scalar bit equality, counts 0..64, exact
write bounds and unchanged source, all tests passed before benchmark,
same seeded input across variants, and injected failure identified as
`THROUGHPUT_FAIL,ADD,u8,blocks=7`.

The experiment adds 1105 comparisons and 357 timing rows to the one-shot
v5 ELF. Timings measure 256 repeated operations across seven sizes and three
rotated-order trials per candidate. Output records source and destination
byte counts separately because RGB565 uses twice as many source bytes.
All candidates preserve o32/n32 argument conventions and use only
volatile register numbers. They are *not* selected as production
implementations until real R5900/PCSX2 results prove an advantage.

A direct fused RGB565 OVER candidate reproduces the exact original
8-pixel MMI body twice per outer iteration. All seven kernel families
are timed only after every regression case passes. Its large code size
may degrade I-cache behavior, which is why it is never selected blindly.
