/* Additional independent, opt-in R5900 MMI throughput candidates.
 * OVER / 8888->565: B=two-quads per loop; A=existing one-quad kernels.
 * A8 IN: B=decode both byte halves before the first PMULTH.
 * 16-byte alignment, nonoverlapping where required, volatile regs only.
 * All input counts are in the same units as the existing EE kernels.
 */
    .text
    .set noreorder
    .align 2
    .globl ee_mmi_over_u2_quads
    .ent ee_mmi_over_u2_quads
ee_mmi_over_u2_quads:
    beqz    $a2, 9f
     nop
    lq      $v0, 0($a3)
    lq      $v1, 16($a3)
1:
    sltiu   $10, $a2, 2
    bnez    $10, 2f
     nop
    lq      $8, 0($a1)
    lq      $9, 0($a0)
    psrlw   $10, $8, 24
    pextlw  $11, $10, $10
    pextuw  $12, $10, $10
    psllw   $24, $11, 16
    por     $11, $11, $24
    pxor    $11, $11, $v0
    psllw   $24, $12, 16
    por     $12, $12, $24
    pxor    $12, $12, $v0
    pextlb  $13, $zero, $9
    pextub  $14, $zero, $9
    pmulth  $15, $13, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $25, $15, 8
    pmulth  $15, $14, $12
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    ppacb   $15, $15, $25
    paddub  $8, $8, $15
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    lq      $8, 0($a1)
    lq      $9, 0($a0)
    psrlw   $10, $8, 24
    pextlw  $11, $10, $10
    pextuw  $12, $10, $10
    psllw   $24, $11, 16
    por     $11, $11, $24
    pxor    $11, $11, $v0
    psllw   $24, $12, 16
    por     $12, $12, $24
    pxor    $12, $12, $v0
    pextlb  $13, $zero, $9
    pextub  $14, $zero, $9
    pmulth  $15, $13, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $25, $15, 8
    pmulth  $15, $14, $12
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    ppacb   $15, $15, $25
    paddub  $8, $8, $15
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
    lq      $8, 0($a1)
    lq      $9, 0($a0)
    psrlw   $10, $8, 24
    pextlw  $11, $10, $10
    pextuw  $12, $10, $10
    psllw   $24, $11, 16
    por     $11, $11, $24
    pxor    $11, $11, $v0
    psllw   $24, $12, 16
    por     $12, $12, $24
    pxor    $12, $12, $v0
    pextlb  $13, $zero, $9
    pextub  $14, $zero, $9
    pmulth  $15, $13, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $25, $15, 8
    pmulth  $15, $14, $12
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    ppacb   $15, $15, $25
    paddub  $8, $8, $15
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
9:
    jr      $ra
     nop
    .end ee_mmi_over_u2_quads
    .align 2
    .globl ee_mmi_in_a8_predecode_quads
    .ent ee_mmi_in_a8_predecode_quads
ee_mmi_in_a8_predecode_quads:
    beqz    $a2, 2f
     nop
    lq      $v1, 0($a3)
1:
    lq      $8, 0($a1)
    lq      $9, 0($a0)
    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    pextub  $24, $zero, $8
    pextub  $25, $zero, $9
    pmulth  $12, $10, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $12
    padduh  $12, $12, $v1
    psrlh   $13, $12, 8
    padduh  $12, $12, $13
    psrlh   $14, $12, 8
    pmulth  $12, $24, $25
    addiu   $a2, $a2, -1
    pmfhl.lh $12
    padduh  $12, $12, $v1
    psrlh   $13, $12, 8
    padduh  $12, $12, $13
    psrlh   $15, $12, 8
    ppacb   $15, $15, $14
    sq      $15, 0($a0)
    addiu   $a0, $a0, 16
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_in_a8_predecode_quads
    .align 2
    .globl ee_mmi_8888_to_0565_u2_quads
    .ent ee_mmi_8888_to_0565_u2_quads
ee_mmi_8888_to_0565_u2_quads:
    beqz    $a2, 9f
     nop
    lq      $v0, 0($a3)
    lq      $v1, 16($a3)
    lq      $25, 32($a3)
1:
    sltiu   $10, $a2, 2
    bnez    $10, 2f
     nop
    lq      $8, 0($a1)
    lq      $9, 16($a1)
    psrlw   $10, $8, 8
    pand    $10, $10, $v0
    psrlw   $11, $8, 5
    pand    $11, $11, $v1
    por     $10, $10, $11
    psrlw   $11, $8, 3
    pand    $11, $11, $25
    por     $10, $10, $11
    psrlw   $12, $9, 8
    pand    $12, $12, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $12, $12, $13
    ppach   $10, $12, $10
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
    lq      $8, 0($a1)
    lq      $9, 16($a1)
    psrlw   $10, $8, 8
    pand    $10, $10, $v0
    psrlw   $11, $8, 5
    pand    $11, $11, $v1
    por     $10, $10, $11
    psrlw   $11, $8, 3
    pand    $11, $11, $25
    por     $10, $10, $11
    psrlw   $12, $9, 8
    pand    $12, $12, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $12, $12, $13
    ppach   $10, $12, $10
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
    lq      $8, 0($a1)
    lq      $9, 16($a1)
    psrlw   $10, $8, 8
    pand    $10, $10, $v0
    psrlw   $11, $8, 5
    pand    $11, $11, $v1
    por     $10, $10, $11
    psrlw   $11, $8, 3
    pand    $11, $11, $25
    por     $10, $10, $11
    psrlw   $12, $9, 8
    pand    $12, $12, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $12, $12, $13
    ppach   $10, $12, $10
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
9:
    jr      $ra
     nop
    .end ee_mmi_8888_to_0565_u2_quads

/* Compare against the direct fused 8-pixel RGB565 OVER baseline.
 * Two consecutive blocks share one branch; all constant loads remain
 * inside the reproduced body because packing clobbers v0/v1.
 * Large code footprint may lose to baseline in EE instruction cache.
 */
    .align 2
    .globl ee_mmi_over_8888_0565_u2_quads
    .ent ee_mmi_over_8888_0565_u2_quads
ee_mmi_over_8888_0565_u2_quads:
    beqz    $a2, 9f
     nop
1:
    sltiu   $8, $a2, 2
    bnez    $8, 2f
     nop
    /* Packing overwrites v0/v1. Reload expansion masks each block. */
    lq      $v0, 0($a3)     /* eight 5-bit masks */
    lq      $v1, 16($a3)    /* eight 6-bit masks */
    lq      $8, 0($a0)     /* eight RGB565 destination pixels */
    lq      $9, 0($a1)     /* source pixels 0..3 */
    lq      $10, 16($a1)    /* source pixels 4..7 */

    /* RGB565 little endian: blue 5, green 6, red 5.
     * Convert using bit replication matching convert_0565_to_0888.
     */
    pand    $11, $8, $v0
    psllh   $12, $11, 3
    psrlh   $11, $11, 2
    por     $12, $12, $11  /* B8 */

    psrlh   $11, $8, 5
    pand    $11, $11, $v1
    psllh   $13, $11, 2
    psrlh   $11, $11, 4
    por     $13, $13, $11  /* G8 */
    psllh   $13, $13, 8
    por     $12, $12, $13  /* 16-bit packed BG */

    psrlh   $11, $8, 11
    psllh   $13, $11, 3
    psrlh   $11, $11, 2
    por     $13, $13, $11  /* R8 */
    pextlh  $11, $13, $12  /* pixels 0..3 in 32-bit BGR0 */
    pextuh  $12, $13, $12  /* pixels 4..7 in 32-bit BGR0 */

    lq      $v0, 32($a3)   /* eight 0x00ff: inverse alpha */
    lq      $v1, 48($a3)   /* eight 0x0080: /255 rounding */

    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $9, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $11
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $11
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $9, $9, $24


    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $10, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $12
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $12
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $10, $10, $24

    /* 8888 -> RGB565: Pixman's truncation, no further rounding.
     * Each 32-bit word now holds a packed 16-bit RGB565 result.
     */
    lq      $v0, 64($a3)   /* 0x0000f800 */
    lq      $v1, 80($a3)   /* 0x000007e0 */
    lq      $25, 96($a3)   /* 0x0000001f */

    psrlw   $11, $9, 8
    pand    $11, $11, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $11, $11, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $11, $11, $13

    psrlw   $12, $10, 8
    pand    $12, $12, $v0
    psrlw   $13, $10, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $10, 3
    pand    $13, $13, $25
    por     $12, $12, $13

    ppach   $11, $12, $11
    sq      $11, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32

    /* Packing overwrites v0/v1. Reload expansion masks each block. */
    lq      $v0, 0($a3)     /* eight 5-bit masks */
    lq      $v1, 16($a3)    /* eight 6-bit masks */
    lq      $8, 0($a0)     /* eight RGB565 destination pixels */
    lq      $9, 0($a1)     /* source pixels 0..3 */
    lq      $10, 16($a1)    /* source pixels 4..7 */

    /* RGB565 little endian: blue 5, green 6, red 5.
     * Convert using bit replication matching convert_0565_to_0888.
     */
    pand    $11, $8, $v0
    psllh   $12, $11, 3
    psrlh   $11, $11, 2
    por     $12, $12, $11  /* B8 */

    psrlh   $11, $8, 5
    pand    $11, $11, $v1
    psllh   $13, $11, 2
    psrlh   $11, $11, 4
    por     $13, $13, $11  /* G8 */
    psllh   $13, $13, 8
    por     $12, $12, $13  /* 16-bit packed BG */

    psrlh   $11, $8, 11
    psllh   $13, $11, 3
    psrlh   $11, $11, 2
    por     $13, $13, $11  /* R8 */
    pextlh  $11, $13, $12  /* pixels 0..3 in 32-bit BGR0 */
    pextuh  $12, $13, $12  /* pixels 4..7 in 32-bit BGR0 */

    lq      $v0, 32($a3)   /* eight 0x00ff: inverse alpha */
    lq      $v1, 48($a3)   /* eight 0x0080: /255 rounding */

    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $9, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $11
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $11
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $9, $9, $24


    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $10, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $12
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $12
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $10, $10, $24

    /* 8888 -> RGB565: Pixman's truncation, no further rounding.
     * Each 32-bit word now holds a packed 16-bit RGB565 result.
     */
    lq      $v0, 64($a3)   /* 0x0000f800 */
    lq      $v1, 80($a3)   /* 0x000007e0 */
    lq      $25, 96($a3)   /* 0x0000001f */

    psrlw   $11, $9, 8
    pand    $11, $11, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $11, $11, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $11, $11, $13

    psrlw   $12, $10, 8
    pand    $12, $12, $v0
    psrlw   $13, $10, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $10, 3
    pand    $13, $13, $25
    por     $12, $12, $13

    ppach   $11, $12, $11
    sq      $11, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32

    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
    /* Packing overwrites v0/v1. Reload expansion masks each block. */
    lq      $v0, 0($a3)     /* eight 5-bit masks */
    lq      $v1, 16($a3)    /* eight 6-bit masks */
    lq      $8, 0($a0)     /* eight RGB565 destination pixels */
    lq      $9, 0($a1)     /* source pixels 0..3 */
    lq      $10, 16($a1)    /* source pixels 4..7 */

    /* RGB565 little endian: blue 5, green 6, red 5.
     * Convert using bit replication matching convert_0565_to_0888.
     */
    pand    $11, $8, $v0
    psllh   $12, $11, 3
    psrlh   $11, $11, 2
    por     $12, $12, $11  /* B8 */

    psrlh   $11, $8, 5
    pand    $11, $11, $v1
    psllh   $13, $11, 2
    psrlh   $11, $11, 4
    por     $13, $13, $11  /* G8 */
    psllh   $13, $13, 8
    por     $12, $12, $13  /* 16-bit packed BG */

    psrlh   $11, $8, 11
    psllh   $13, $11, 3
    psrlh   $11, $11, 2
    por     $13, $13, $11  /* R8 */
    pextlh  $11, $13, $12  /* pixels 0..3 in 32-bit BGR0 */
    pextuh  $12, $13, $12  /* pixels 4..7 in 32-bit BGR0 */

    lq      $v0, 32($a3)   /* eight 0x00ff: inverse alpha */
    lq      $v1, 48($a3)   /* eight 0x0080: /255 rounding */

    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $9, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $11
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $11
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $9, $9, $24


    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $13, $10, 24
    pextlw  $14, $13, $13
    pextuw  $15, $13, $13
    psllw   $24, $14, 16
    por     $14, $14, $24
    pxor    $14, $14, $v0
    psllw   $24, $15, 16
    por     $15, $15, $24
    pxor    $15, $15, $v0

    pextlb  $13, $zero, $12
    pmulth  $25, $13, $14
    pmfhl.lh $25
    padduh  $25, $25, $v1
    psrlh   $24, $25, 8
    padduh  $25, $25, $24
    psrlh   $25, $25, 8

    pextub  $13, $zero, $12
    pmulth  $24, $13, $15
    pmfhl.lh $24
    padduh  $24, $24, $v1
    psrlh   $13, $24, 8
    padduh  $24, $24, $13
    psrlh   $24, $24, 8
    ppacb   $24, $24, $25
    paddub  $10, $10, $24

    /* 8888 -> RGB565: Pixman's truncation, no further rounding.
     * Each 32-bit word now holds a packed 16-bit RGB565 result.
     */
    lq      $v0, 64($a3)   /* 0x0000f800 */
    lq      $v1, 80($a3)   /* 0x000007e0 */
    lq      $25, 96($a3)   /* 0x0000001f */

    psrlw   $11, $9, 8
    pand    $11, $11, $v0
    psrlw   $13, $9, 5
    pand    $13, $13, $v1
    por     $11, $11, $13
    psrlw   $13, $9, 3
    pand    $13, $13, $25
    por     $11, $11, $13

    psrlw   $12, $10, 8
    pand    $12, $12, $v0
    psrlw   $13, $10, 5
    pand    $13, $13, $v1
    por     $12, $12, $13
    psrlw   $13, $10, 3
    pand    $13, $13, $25
    por     $12, $12, $13

    ppach   $11, $12, $11
    sq      $11, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32

9:
    jr      $ra
     nop
    .end ee_mmi_over_8888_0565_u2_quads
