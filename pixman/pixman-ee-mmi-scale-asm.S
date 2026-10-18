/* R5900 Emotion Engine: scaled pixel fast paths.
 *
 * ee_mmi_nearest_opaque_quads:
 *   a0=16B dst, a1=16B XRGB source, a2=quad count,
 *   a3=aligned 4-word alpha bit mask. Emits opaque 8888 pixels.
 *
 * ee_mmi_bilinear_lerp_quads:
 *   a0=aligned output, a1=aligned first tap, a2=aligned second tap,
 *   a3=aligned 64-byte coefficients per 4 pixels, 5th o32 argument at
 *   16(sp) = quad count. Coefficients have 16-bit lane values in [0,256],
 *   sum to exactly 256 per byte channel. For each quad:
 *     +0: w0 for pixels 0,1 (8 halfwords)
 *    +16: w0 for pixels 2,3
 *    +32: w1 for pixels 0,1
 *    +48: w1 for pixels 2,3
 *   round() is truncation: (tap0*w0 + tap1*w1) >> 8.
 * All intermediate products/sums fit in unsigned 16 bits (<= 65280),
 * so PMFHL.LH -> PADDUH -> PSRLH does not lose precision.
 * The caller handles four-tap filtering, boundary repeat and overlap.
 *
 * Both entry points clobber only caller-saved GPRs. All LQ/SQ are aligned,
 * and every branch has an explicit delay-slot NOP.
 */
    .text
    .set noreorder

    .align 2
    .globl ee_mmi_nearest_opaque_quads
    .ent ee_mmi_nearest_opaque_quads
ee_mmi_nearest_opaque_quads:
    beqz    $a2, 2f
     nop
    lq      $8, 0($a3)
1:
    lq      $9, 0($a1)
    por     $9, $9, $8
    sq      $9, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_nearest_opaque_quads

    .align 2
    .globl ee_mmi_bilinear_lerp_quads
    .ent ee_mmi_bilinear_lerp_quads
ee_mmi_bilinear_lerp_quads:
    #if _MIPS_SIM == _ABIN32
    move    $25, $8 /* n32 fifth argument */
    #else
    lw      $25, 16($sp)
    #endif
    beqz    $25, 2f
     nop
1:
    lq      $8, 0($a1)
    lq      $9, 0($a2)

    /* First two pixels, eight 16-bit lanes. */
    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    lq      $12, 0($a3)
    lq      $13, 32($a3)
    pmulth  $14, $10, $12
    pmfhl.lh $14
    pmulth  $15, $11, $13
    pmfhl.lh $15
    padduh  $14, $14, $15
    psrlh   $v0, $14, 8

    /* Last two pixels. */
    pextub  $10, $zero, $8
    pextub  $11, $zero, $9
    lq      $12, 16($a3)
    lq      $13, 48($a3)
    pmulth  $14, $10, $12
    pmfhl.lh $14
    pmulth  $15, $11, $13
    pmfhl.lh $15
    padduh  $14, $14, $15
    psrlh   $v1, $14, 8
    ppacb   $24, $v1, $v0

    sq      $24, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, 16
    addiu   $a3, $a3, 64
    addiu   $25, $25, -1
    bnez    $25, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_bilinear_lerp_quads

/* Exact 4-tap bilinear support: horizontal numerators + vertical finish.
 * Horizontal input: four packed 8888 pixels each from left/right,
 * weights layout identical to ee_mmi_bilinear_lerp_quads.
 * Horizontal output: eight 16-bit channels per 16-byte quad,
 * two 16-byte stores (= 4 pixels) for each input quad.
 * a0=aligned horizontal sum out, a1=aligned left, a2=aligned right,
 * a3=aligned 64B/quad weights, fifth o32 arg at 16(sp)=quad count.
 */
    .align 2
    .globl ee_mmi_bilinear_hsum_quads
    .ent ee_mmi_bilinear_hsum_quads
ee_mmi_bilinear_hsum_quads:
    #if _MIPS_SIM == _ABIN32
    move    $25, $8 /* n32 fifth argument */
    #else
    lw      $25, 16($sp)
    #endif
    beqz    $25, 2f
     nop
1:
    lq      $8, 0($a1)
    lq      $9, 0($a2)

    /* Horizontal pixels 0 and 1: preserve the
     * full /256 numerator in unsigned 16-bit lanes, without truncation.
     */
    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    lq      $12, 0($a3)
    lq      $13, 32($a3)
    pmulth  $14, $10, $12
    pmfhl.lh $14
    pmulth  $15, $11, $13
    pmfhl.lh $15
    padduh  $14, $14, $15
    sq      $14, 0($a0)


    /* Horizontal pixels 2 and 3: preserve the
     * full /256 numerator in unsigned 16-bit lanes, without truncation.
     */
    pextub  $10, $zero, $8
    pextub  $11, $zero, $9
    lq      $12, 16($a3)
    lq      $13, 48($a3)
    pmulth  $14, $10, $12
    pmfhl.lh $14
    pmulth  $15, $11, $13
    pmfhl.lh $15
    padduh  $14, $14, $15
    sq      $14, 16($a0)

    addiu   $a0, $a0, 32
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, 16
    addiu   $a3, $a3, 64
    addiu   $25, $25, -1
    bnez    $25, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_bilinear_hsum_quads

/* Exact vertical finish from two aligned 16-bit horizontal numerators.
 * a0=aligned 8888 dst, a1=aligned top H[4*4 halfwords],
 * a2=aligned bottom H[4*4 halfwords],
 * a3=aligned [wY0 x8, wY1 x8, 255 x8] halfword vectors,
 * fifth o32 argument at 16(sp)=quad count.
 *
 * Algebra exactly equals Pixman's 7-bit four-tap integer interpolation:
 * ((TL*(256-2*dx)+TR*2*dx)*(256-2*dy)
 * +(BL*(256-2*dx)+BR*2*dx)*2*dy) >> 16.
 */
    .align 2
    .globl ee_mmi_bilinear_vfinish_quads
    .ent ee_mmi_bilinear_vfinish_quads
ee_mmi_bilinear_vfinish_quads:
    #if _MIPS_SIM == _ABIN32
    move    $25, $8 /* n32 fifth argument */
    #else
    lw      $25, 16($sp)
    #endif
    beqz    $25, 2f
     nop
    lq      $v0, 0($a3)
    lq      $v1, 16($a3)
    lq      $a3, 32($a3)
1:

    /* Vertical first pair of pixels. For each
     * horizontal 16-bit numerator H = 256*H_hi + H_lo:
     * value = (H_hi*w0 + H2_hi*w1 +
     *         ((H_lo*w0 + H2_lo*w1) >> 8)) >> 8.
     * No 16-bit product, low carry or final sum can overflow.
     */
    lq      $8, 0($a1)
    lq      $9, 0($a2)
    psrlh   $10, $8, 8
    psrlh   $11, $9, 8
    pmulth  $14, $10, $v0
    pmfhl.lh $14
    pmulth  $15, $11, $v1
    pmfhl.lh $15
    padduh  $14, $14, $15
    pand    $10, $8, $a3
    pand    $11, $9, $a3
    pmulth  $15, $10, $v0
    pmfhl.lh $15
    pmulth  $10, $11, $v1
    pmfhl.lh $10
    padduh  $15, $15, $10
    psrlh   $15, $15, 8
    padduh  $14, $14, $15
    psrlh   $24, $14, 8


    /* Vertical second pair of pixels. For each
     * horizontal 16-bit numerator H = 256*H_hi + H_lo:
     * value = (H_hi*w0 + H2_hi*w1 +
     *         ((H_lo*w0 + H2_lo*w1) >> 8)) >> 8.
     * No 16-bit product, low carry or final sum can overflow.
     */
    lq      $8, 16($a1)
    lq      $9, 16($a2)
    psrlh   $10, $8, 8
    psrlh   $11, $9, 8
    pmulth  $14, $10, $v0
    pmfhl.lh $14
    pmulth  $15, $11, $v1
    pmfhl.lh $15
    padduh  $14, $14, $15
    pand    $10, $8, $a3
    pand    $11, $9, $a3
    pmulth  $15, $10, $v0
    pmfhl.lh $15
    pmulth  $10, $11, $v1
    pmfhl.lh $10
    padduh  $15, $15, $10
    psrlh   $15, $15, 8
    padduh  $14, $14, $15
    psrlh   $14, $14, 8

    ppacb   $14, $14, $24
    sq      $14, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, 32
    addiu   $25, $25, -1
    bnez    $25, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_bilinear_vfinish_quads
