# PlayStation 2 EE MMI backend

This backend is for the **R5900 Emotion Engine** 128-bit Multimedia
Instructions. It is **not** the Loongson MMI implementation
(`-Dloongson-mmi`), and is not the MIPS32 DSP R2 implementation.

## Build and selection

With a PS2 EE-targeted compiler and assembler, use:

```sh
meson setup build-ee --cross-file path/to/ps2-ee-cross.txt -Dee-mmi=enabled
```

The `ee-mmi` feature is `auto` by default. Meson enables it only on MIPS
targets whose assembler accepts EE `LQ`, `SQ`, and `PADDUB` instructions.
The compiler and cross-file must already target the EE; the option does not
select a CPU, compiler, or assembler.

To compare with Pixman's existing C paths, set `PIXMAN_DISABLE=ee-mmi` when
running an application or test. The build-time `-Dee-mmi=disabled` option
also removes the backend.

## Supported operations

* Saturating `ADD`: ARGB8888 to ARGB8888, ABGR8888 to ABGR8888,
  and A8 to A8.
* `SRC`: 32-bit same-format copies for ARGB8888, ABGR8888,
  XRGB8888, and XBGR8888.
* 32bpp solid fills through Pixman's `fill` implementation hook.

All other operations continue through the normal Pixman fallback chain.
In particular, `OVER`, component-alpha, bilinear scaling, and RGB565
conversion are **not** accelerated in this initial patch.

## Alignment and correctness

EE `LQ`/`SQ` silently mask the lowest four address bits. Every 128-bit
assembly access is therefore gated by C-side 16-byte alignment checks.
The row prefix and tail are processed in C. If the source and destination
have incompatible alignments, small spans use C instead. Overlapping ADD
spans use forward scalar processing and overlapping SRC spans use memmove.

The ADD kernel uses `PADDUB` saturating 8-bit addition, matching Pixman's
channel-wise ADD arithmetic exactly. The implementation never assumes that
the image row stride is positive or that rows begin 16-byte aligned.

## Validation still required

The author has not run PS2 compilation, tests, or performance benchmarks.
Before deploying, verify the target toolchain's assembler and o32 ABI
conventions and compare against `PIXMAN_DISABLE=ee-mmi`.

Recommended checks:

* `test/composite.c`, `test/combiner-test.c`, `test/blitters-test.c`
  on actual PS2 hardware or an EE-capable emulator.
* Pixel values near 0 and 255; 1-, 2-, 3-, 4-, and 16-pixel widths.
* Src/Dst alignment offsets 0..15, odd and negative strides, clipped regions.
* Both identical pointers and partially overlapping buffers.
* Sizes and memory layouts representative of the actual application.

For performance, measure separate small/large rectangles and cache-warm/cold
cases. The transfer paths are likely to be limited by EE memory bandwidth,
so wins over the C or libc paths are not guaranteed.
