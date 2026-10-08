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
    lq      $t0, 0($a3)
1:
    lq      $t1, 0($a1)
    por     $t1, $t1, $t0
    sq      $t1, 0($a0)
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
    lw      $t9, 16($sp)
    beqz    $t9, 2f
     nop
1:
    lq      $t0, 0($a1)
    lq      $t1, 0($a2)

    /* First two pixels, eight 16-bit lanes. */
    pextlb  $t2, $zero, $t0
    pextlb  $t3, $zero, $t1
    lq      $t4, 0($a3)
    lq      $t5, 32($a3)
    pmulth  $t6, $t2, $t4
    pmfhl.lh $t6
    pmulth  $t7, $t3, $t5
    pmfhl.lh $t7
    padduh  $t6, $t6, $t7
    psrlh   $v0, $t6, 8

    /* Last two pixels. */
    pextub  $t2, $zero, $t0
    pextub  $t3, $zero, $t1
    lq      $t4, 16($a3)
    lq      $t5, 48($a3)
    pmulth  $t6, $t2, $t4
    pmfhl.lh $t6
    pmulth  $t7, $t3, $t5
    pmfhl.lh $t7
    padduh  $t6, $t6, $t7
    psrlh   $v1, $t6, 8
    ppacb   $t8, $v1, $v0

    sq      $t8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, 16
    addiu   $a3, $a3, 64
    addiu   $t9, $t9, -1
    bnez    $t9, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_bilinear_lerp_quads
