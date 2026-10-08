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
* `IN` blending from A8 into A8 using two 128-bit loads,
  zero-extend, 16-bit PMULTH, PMFHL.LH, exact Pixman /255 rounding,
  PPACB packing and one 128-bit store per 16 pixels.
* Nearest-neighbor scaled `OVER` for 32-bit ARGB/ABGR/XRGB/XBGR8888
  into matching 32-bit destinations, using Pixman's original nearest
  transform/repeat sampler followed by 64-pixel MMI OVER batches.
  Reads/writes that share the same image bits stay scalar.
* Bilinear scaled `OVER` for the same 32-bit format families, using
  Pixman's `bilinear_interpolation` and fractional weights, with
  four taps from Pixman's repeat rules. The expensive blend is batched
  through the EE MMI OVER kernel. Aliased source/destination stays scalar.
* Unmasked same-format `SRC` copying of A8 and RGB565/BGR565,
  in addition to the existing 32-bit formats.
* Direct `pixman_blt` for 8/16/32bpp copying, using EE 128-bit
  memory transfers when alignment and size permit. Same-buffer
  vertical overlap copies rows in the correct direction.
* Generic 32-bit unified-alpha `OVER` combiner hook, allowing
  MMI blending even when no specialized fast path matches.
* `SRC`: 32-bit same-format copies for ARGB8888, ABGR8888,
  XRGB8888, and XBGR8888.
* 8bpp A8, 16bpp RGB565/BGR565, and 32bpp solid fills through Pixman's
  `fill` implementation hook (16-bit patterns are replicated eight times).
* Component-alpha `OVER` for solid + 32bit ARGB/ABGR CA mask ->
  ARGB/XRGB/ABGR/XBGR8888. A fused four-pixel EE MMI path performs mask
  scaling, inverse alpha, and saturating destination composition without
  intermediate buffers when the mask and destination have compatible
  alignment. The previous staged MMI path handles incompatible alignment;
  overlapping data continues to use scalar forward traversal.
* Premultiplied-alpha `OVER` for ARGB8888 / ABGR8888 to matching 32-bit
  destinations (including the XRGB8888/XBGR8888 destination variants).
* `OVER` for solid + A8 coverage mask -> ARGB8888 / XRGB8888 /
  ABGR8888 / XBGR8888. A fused four-pixel EE MMI kernel expands
  four A8 bytes in registers and performs exact masked-source and OVER
  blending without the old 1 KiB LUT or 256-byte staging buffer.
  A8 masks may be unaligned; overlapping mask/destination views and
  short spans retain the scalar forward-traversal fallback.
* `OVER` for 8888 source + A8 coverage mask into 8888 destination:
  aligned ARGB/ABGR source/destination use an EE MMI four-pixel fusion
  of source scaling, dynamic alpha and saturated destination blending.
  XRGB/XBGR and incompatible alignments retain the staged MMI path;
  overlapping source/mask/destination views remain forward scalar.
* Unmasked solid-color `OVER` into 8888, using an aligned repeated
  source buffer and the same EE MMI kernel.
* Unmasked `OVER` from ARGB8888 into RGB565 and from ABGR8888 into
  BGR565. When both scanlines align at a 16-byte boundary, a fused EE
  MMI eight-pixel kernel expands RGB565, performs exact /255 OVER and
  repacks RGB565 without scratch. Incompatible source alignment retains
  the original eight-pixel staged MMI implementation.
* `SRC` from ARGB8888 / XRGB8888 to RGB565, and from ABGR8888 /
  XBGR8888 to BGR565.

All other operations continue through the normal Pixman fallback chain.
Unsupported mask formats, non-solid-source component-alpha, and
scaled SRC are **not** accelerated. Scaled OVER (nearest and bilinear)
uses scalar sampling with MMI blending, so speedups are not guaranteed.
RGB565 OVER uses the fused register-only kernel for aligned scanlines
and keeps the original staged scalar-expansion fallback for other
alignments; both require workload-specific benchmarks.

## Alignment and correctness

EE `LQ`/`SQ` silently mask the lowest four address bits. Every 128-bit
assembly access is therefore gated by C-side 16-byte alignment checks.
The row prefix and tail are processed in C. If the source and destination
have incompatible alignments, small spans use C instead. Overlapping ADD
spans use forward scalar processing and overlapping SRC spans use memmove.

The ADD kernel uses `PADDUB` saturating 8-bit addition, matching Pixman's
channel-wise ADD arithmetic exactly. The OVER kernel unpacks destination
bytes, multiplies by repeated `255 - src_alpha` with `PMULTH`, extracts
eight low product halves with `PMFHL.LH`, performs Pixman's exact
`(x + 128 + ((x + 128) >> 8)) >> 8` rounded division by 255, then
adds premultiplied source channels with `PADDUB`. The implementation never assumes that
the image row stride is positive or that rows begin 16-byte aligned.

## Validation still required

The author has not run PS2 compilation, tests, or performance benchmarks.
Before deploying, verify the target toolchain's assembler and o32 ABI
conventions and compare against `PIXMAN_DISABLE=ee-mmi`.

Recommended checks:

* `test/composite.c`, `test/combiner-test.c`, `test/blitters-test.c`
  on actual PS2 hardware or an EE-capable emulator.
* Pixel values near 0 and 255; 1-, 2-, 3-, 4-, and 16-pixel widths.
* OVER with alpha 0, 1, 127, 254 and 255 and mixed transparency.
* Direct pixman_blt: byte/pixel alignment, positive/negative stride,
  identical source/destination images, horizontal and vertical overlap.
* Generic 32-bit OVER combiner with NULL and unified-alpha masks,
  transparent source and mask pixels, and overlapping scanlines.
* RGB565 conversion with source and destination independently offset by
  0 through 15 bytes; confirm color channel order against the C fast path.
* Solid+A8 mask with all coverage values 0..255, transparent and opaque
  solid sources, mask/destination aliasing, very small widths, and negative
  strides.
* Image+A8 mask with all coverage values 0..255, source alpha 0/255,
  XRGB/XBGR source with arbitrary stored high bytes, source/destination
  and mask/destination aliasing, and different alignment offsets.
* Solid unmasked OVER with premultiplied colors, alignment offsets,
  short and long rectangles, and both opaque and translucent colors.
* RGB565 OVER with mixed-alpha premultiplied input, source/destination
  aliasing, short spans, and destination alignment offsets 0..14 by 2.
* 8- and 16-bit fill for every possible x alignment, odd widths, and
  negative strides.
* Bilinear scaling: PAD/NONE/NORMAL/REFLECT repeat, edge samples,
  partially transparent bilinear taps, transformed negative coordinates,
  negative scale factors, and bilinear weights around 0 and 255.
* Nearest-neighbor scaling: PAD/NONE/NORMAL/REFLECT repeat, negative
  scales, transparent out-of-bounds samples, image aliasing, 16-byte
  alignment, source with ignored high alpha bytes, and coordinate
  rounding around half-pixel boundaries.
* A8 IN: all source/destination alpha byte combinations, 16-byte
  alignment offsets, source/destination overlap and identical pointers.
* Component-alpha solid OVER: each of the 4 mask channels at boundaries,
  opaque source, zero/full masks, partial mask alpha, negative strides,
  row alignment, and overlapping mask/destination image buffers.
* Src/Dst alignment offsets 0..15, odd and negative strides, clipped regions.
* Both identical pointers and partially overlapping buffers.
* Sizes and memory layouts representative of the actual application.

For performance, measure separate small/large rectangles and cache-warm/cold
cases. The transfer paths are likely to be limited by EE memory bandwidth,
so wins over the C or libc paths are not guaranteed.

## Fused component-alpha regression test

Use `test/ee-mmi-ca-fused-test.c` to check the actual fused EE assembly
against independent scalar output, including all-zero/opaque and random
mask channels, randomized destination values, block counts 0..32, and
write canaries. Compile with the EE toolchain together with
`pixman/pixman-ee-mmi-ca-asm.S`. The original Pixman composite tests
must also cover unaligned and overlapping masks/destinations.
Performance on PS2 hardware and PCSX2 has not yet been measured.

## Fused solid + A8 regression test

The standalone `test/ee-mmi-a8-fused-test.c` compares the actual EE assembly
against a scalar exact-/255 reference. It tests all 256 coverage values,
unaligned A8 mask pointers (offsets 0..15), 0..32 MMI blocks, transparent
and opaque solids, randomized destinations, and untouched canary bytes.
Build with an EE-targeted compiler together with
`pixman/pixman-ee-mmi-a8-asm.S`. Run on PCSX2 and real hardware.
The existing Pixman composite tests remain necessary for sub-16-pixel
spans, clipping, negative strides and overlapping mask/destination buffers.
Measure wall time/cycles relative to `PIXMAN_DISABLE=ee-mmi`: removing
staging does not guarantee a speedup on the R5900.

## Image+A8 fused MMI differential test

`test/ee-mmi-image-a8-fused-test.c` directly exercises the new assembly
kernel with 16,000 source/destination random and alpha-edge test cases,
coverage 0..255, arbitrary A8 byte alignment 0..15, and whole-array input
and destination-canary checks. The fifth o32 argument is read at 16(sp).
