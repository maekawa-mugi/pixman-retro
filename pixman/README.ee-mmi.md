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
* Nearest-neighbor scaled `OVER` for ARGB/ABGR/XRGB/XBGR8888.
  When horizontal sampling is unit scale, entirely within source bounds,
  and source/destination have congruent 16-byte alignment, the renderer
  now composes directly from the source scanline with EE MMI instead of
  gathering into 64-pixel scratch. Opaque XRGB/XBGR uses a new quadword
  alpha-forcing copy kernel. Other scales and repeat edge cases retain
  the existing MMI-staged pixel gather; aliased images stay scalar.
* Bilinear scaled `OVER` for the same families. Exact 2-tap EE MMI
  handles horizontal-only filtering (zero vertical fraction) and
  vertical-only filtering (zero horizontal fraction with integral
  source-X step). A new **exact 4-tap EE MMI pipeline** accelerates
  wide (>= 64 pixels remaining), non-aliased general 2D filtering:
  horizontal PMULTH generates full 16-bit numerators for both source
  rows, and a vertical PMULTH/shift/carry pass evaluates the original
  Pixman /65536 expression without intermediate rounding. Four pixels
  are processed per quad. Small spans remain with Pixman's scalar
  four-tap interpolation. Bulk paths cache Y-repeat per scanline and
  reuse X-repeat tests; blending still uses EE MMI. Aliased images
  stay scalar.
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
* Component-alpha `OVER` for image + matching 32-bit ARGB/ABGR
  CA mask -> ARGB/XRGB/ABGR/XBGR8888. A 3-input EE MMI four-pixel
  kernel fuses per-pixel source alpha, channel-wise multiplication, and
  saturated destination blend. Incompatible alignment and overlap stay C.
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

## Validation

The backend has been exercised only in an out-of-tree test suite; on-target
speedups depend on the workload and are not guaranteed. Compare against
`PIXMAN_DISABLE=ee-mmi` and run `test/composite.c`, `test/combiner-test.c`
and `test/blitters-test.c` on PS2 hardware or an EE-capable emulator before
deploying.
