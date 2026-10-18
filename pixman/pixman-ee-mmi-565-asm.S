/* PS2 Emotion Engine: fused premultiplied 8888 -> 565 OVER.
 * Process eight pixels per 16-byte destination quad. All LQ/SQ operands
 * are aligned. a0=dst565, a1=src8888, a2=8-pixel block count,
 * a3=aligned 112-byte constant table:
 *    0x00: 8x 0x001f, 0x10: 8x 0x003f,
 *    0x20: 8x 0x00ff, 0x30: 8x 0x0080,
 *    0x40: 4x 0x0000f800, 0x50: 4x 0x000007e0,
 *    0x60: 4x 0x0000001f.
 *
 * Expand 565 to Pixman's exact RGB888 channels by bit replication.
 * Destination alpha is zero. Blend with rounded /255 and saturated
 * PADDUB. Pack to 565 in registers (two 8888 quads -> one 565 quad).
 * Only caller-saved GPRs; PMULTH / PMFHL.LH are paired.
 */
    .text
    .set noreorder

    .align 2
    .globl ee_mmi_over_8888_0565_quads
    .ent ee_mmi_over_8888_0565_quads
ee_mmi_over_8888_0565_quads:
    beqz    $a2, 2f
     nop
1:
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
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_8888_0565_quads
