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
* Unmasked same-format `SRC` copying of A8 and RGB565/BGR565,
  in addition to the existing 32-bit formats.
* `SRC`: 32-bit same-format copies for ARGB8888, ABGR8888,
  XRGB8888, and XBGR8888.
* 8bpp A8, 16bpp RGB565/BGR565, and 32bpp solid fills through Pixman's
  `fill` implementation hook (16-bit patterns are replicated eight times).
* Component-alpha `OVER` for solid + 32bit ARGB/ABGR CA mask ->
  ARGB/XRGB/ABGR/XBGR8888. Preprocess exact channel-wise Pixman
  multiplication in C, then use a dedicated EE MMI PMULTH/PADDUB
  destination blend kernel for batches of four pixels.
* Premultiplied-alpha `OVER` for ARGB8888 / ABGR8888 to matching 32-bit
  destinations (including the XRGB8888/XBGR8888 destination variants).
* `OVER` for solid + A8 coverage mask -> ARGB8888 / XRGB8888 /
  ABGR8888 / XBGR8888, with a one-kilobyte mask lookup table and
  batched MMI OVER; short spans stay scalar.
* `OVER` for 8888 source + A8 coverage mask into 8888 destination:
  64-pixel batches of premultiplied masked source values feed the
  same four-pixel EE MMI OVER kernel. Includes XRGB/XBGR opaque-source
  support, with unrelated image/mask/destination alignments.
* Unmasked solid-color `OVER` into 8888, using an aligned repeated
  source buffer and the same EE MMI kernel.
* Unmasked `OVER` from ARGB8888 into RGB565 and from ABGR8888 into
  BGR565, using an aligned eight-pixel scratch block to reuse the
  existing MMI OVER and RGB565 packing kernels.
* `SRC` from ARGB8888 / XRGB8888 to RGB565, and from ABGR8888 /
  XBGR8888 to BGR565.

All other operations continue through the normal Pixman fallback chain.
Unsupported mask formats, non-solid-source component-alpha, bilinear
scaling, and scaled SRC (including nearest SRC) are **not** accelerated. The RGB565 OVER implementation performs
scalar RGB565 expansion followed by two MMI OVER blocks and an MMI
conversion block; its performance is workload-dependent and may not
beat Pixman's original scalar routine.

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
