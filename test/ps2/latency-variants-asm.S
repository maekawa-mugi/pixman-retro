/* R5900 / PS2SDK n32 and PS2 Linux o32, experiment-only kernels.
 * Independent load scheduling to avoid avoidable load-use interlocks.
 * Physical caller-saved register numbers $8..$15,$24,$25 throughout.
 *
 * Nearest variants: a0 = aligned dst, a1 = aligned XRGB src,
 * a2 = 16-byte quad count, a3 = aligned repeated alpha OR-mask.
 * No memory is read or written for count 0. Overlapping buffers are
 * NOT supported (same precondition as the existing direct copy).
 */
    .text
    .set noreorder
    .align 2
    .globl ee_mmi_nearest_opaque_u2_quads
    .ent ee_mmi_nearest_opaque_u2_quads
ee_mmi_nearest_opaque_u2_quads:
    beqz    $a2, 3f
     nop
    lq      $8, 0($a3)
1:
    sltiu   $9, $a2, 2
    bnez    $9, 2f
     nop
    lq      $10, 0($a1)
    lq      $11, 16($a1)
    por     $10, $10, $8
    por     $11, $11, $8
    sq      $10, 0($a0)
    sq      $11, 16($a0)
    addiu   $a0, $a0, 32
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 3f
     nop
    lq      $10, 0($a1)
    por     $10, $10, $8
    sq      $10, 0($a0)
3:
    jr      $ra
     nop
    .end ee_mmi_nearest_opaque_u2_quads

    .align 2
    .globl ee_mmi_nearest_opaque_u4_quads
    .ent ee_mmi_nearest_opaque_u4_quads
ee_mmi_nearest_opaque_u4_quads:
    beqz    $a2, 4f
     nop
    lq      $8, 0($a3)
1:
    sltiu   $9, $a2, 4
    bnez    $9, 2f
     nop
    lq      $10, 0($a1)
    lq      $11, 16($a1)
    lq      $12, 32($a1)
    lq      $13, 48($a1)
    por     $10, $10, $8
    por     $11, $11, $8
    por     $12, $12, $8
    por     $13, $13, $8
    sq      $10, 0($a0)
    sq      $11, 16($a0)
    sq      $12, 32($a0)
    sq      $13, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a1, $a1, 64
    addiu   $a2, $a2, -4
    b       1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    lq      $10, 0($a1)
    por     $10, $10, $8
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
4:
    jr      $ra
     nop
    .end ee_mmi_nearest_opaque_u4_quads

/* Bilinear horizontal 4-pixel kernel, scheduled variant:
 * a0=32-byte output per quad (aligned uint16 numerators), a1/a2 =
 * aligned 8888 tap rows, a3=64-byte weights per quad; fifth argument
 * = block count ($8 for n32, 16($sp) for o32).
 *
 * Preload both halves' coefficients and schedule independent upper-half
 * byte extraction between PMULTH and PMFHL.LH, rather than inserting
 * artificial NOPs. HI/LO is always consumed before the next PMULTH.
 * All 16-bit sums <= 65280, bit-exact with baseline HSUM.
 */
    .align 2
    .globl ee_mmi_bilinear_hsum_sched_quads
    .ent ee_mmi_bilinear_hsum_sched_quads
ee_mmi_bilinear_hsum_sched_quads:
#if _MIPS_SIM == _ABIN32
    move    $25, $8
#else
    lw      $25, 16($sp)
#endif
    beqz    $25, 2f
     nop
1:
    lq      $8, 0($a1)
    lq      $9, 0($a2)
    lq      $12, 0($a3)
    lq      $13, 16($a3)
    lq      $14, 32($a3)
    lq      $15, 48($a3)

    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    pmulth  $24, $10, $12
    pextub  $10, $zero, $8
    pmfhl.lh $24
    pmulth  $2, $11, $14
    pextub  $11, $zero, $9
    pmfhl.lh $2
    padduh  $24, $24, $2
    sq      $24, 0($a0)

    pmulth  $24, $10, $13
    pmfhl.lh $24
    pmulth  $2, $11, $15
    pmfhl.lh $2
    padduh  $24, $24, $2
    sq      $24, 16($a0)

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
    .end ee_mmi_bilinear_hsum_sched_quads

/* Bilinear HSUM C alternative: decode both the low and high byte lanes
 * before launching either PMULTH. This trades extra live registers for
 * less source-decode dependency near the multiply.
 *
 * Same signature and o32/n32 argument rules as HSum scheduled variant.
 * All LQ/SQ are 16-byte aligned; the 4-pixel result occupies 32 bytes.
 * This is a measured candidate only, NOT the production kernel.
 */
    .align 2
    .globl ee_mmi_bilinear_hsum_predecode_quads
    .ent ee_mmi_bilinear_hsum_predecode_quads
ee_mmi_bilinear_hsum_predecode_quads:
#if _MIPS_SIM == _ABIN32
    move    $25, $8
#else
    lw      $25, 16($sp)
#endif
    beqz    $25, 2f
     nop
1:
    lq      $8, 0($a1)
    lq      $9, 0($a2)
    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    pextub  $24, $zero, $8
    pextub  $2, $zero, $9
    lq      $12, 0($a3)
    lq      $13, 16($a3)
    lq      $14, 32($a3)
    lq      $15, 48($a3)

    pmulth  $3, $10, $12
    pmfhl.lh $3
    pmulth  $8, $11, $14
    pmfhl.lh $8
    padduh  $3, $3, $8
    sq      $3, 0($a0)

    pmulth  $3, $24, $13
    pmfhl.lh $3
    pmulth  $8, $2, $15
    pmfhl.lh $8
    padduh  $3, $3, $8
    sq      $3, 16($a0)

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
    .end ee_mmi_bilinear_hsum_predecode_quads
