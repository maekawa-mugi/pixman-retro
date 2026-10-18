/*
 * PlayStation 2 Emotion Engine 128-bit MMI: OVER and 8888 -> 0565.
 * o32 ABI: a0 dst, a1 src, a2 block count, a3 aligned constants.
 * Caller-saved GPRs only. PMULTH writes the EE 128-bit HI/LO pair.
 * All memory operands of LQ/SQ are 16-byte aligned.
 */
    .text
    .set noreorder

/* Four premultiplied 8888 pixels per 16-byte block.
 *
 * Alpha replication uses PSRLW, PEXTLW/PEXTUW, PSLLW and POR.
 * PXOR with 0x00ff yields (255 - alpha) in each 16-bit halfword.
 *
 * PMULTH produces eight signed-16 x signed-16 products, each <=65025.
 * PMFHL.LH extracts their low 16 bits (the full product fits in 16 bits).
 * Rounded division by 255, exactly as Pixman:
 *       v = product + 128; v = (v + (v >> 8)) >> 8.
 * The maximum intermediate is 65407, so PADDUH does not saturate.
 */
    .align 2
    .globl ee_mmi_over_quads
    .ent ee_mmi_over_quads
ee_mmi_over_quads:
    beqz    $a2, 2f
     nop
    lq      $v0, 0($a3)       /* eight 0x00ff halfwords */
    lq      $v1, 16($a3)      /* eight 0x0080 halfwords */
1:
    lq      $8, 0($a1)       /* source pixels 0 .. 3 */
    lq      $9, 0($a0)       /* destination pixels 0 .. 3 */
    psrlw   $10, $8, 24      /* alpha per 32-bit pixel */
    pextlw  $11, $10, $10     /* repeat alpha words of pixel 0/1 */
    pextuw  $12, $10, $10     /* repeat alpha words of pixel 2/3 */
    psllw   $24, $11, 16
    por     $11, $11, $24     /* repeat alpha to each 16-bit lane */
    pxor    $11, $11, $v0     /* inverse alpha, pixels 0/1 */
    psllw   $24, $12, 16
    por     $12, $12, $24
    pxor    $12, $12, $v0     /* inverse alpha, pixels 2/3 */
    pextlb  $13, $zero, $9   /* zero-extend first 8 destination bytes */
    pextub  $14, $zero, $9   /* zero-extend last 8 destination bytes */

    pmulth  $15, $13, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $25, $15, 8      /* two pixels in eight 16-bit lanes */

    pmulth  $15, $14, $12
    addiu   $a2, $a2, -1
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    ppacb   $15, $15, $25    /* low 64-bit: pixel 0/1, high: 2/3 */
    paddub  $8, $8, $15   /* src + rounded(dst * inv_alpha) */
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_quads

/* Eight 8888 pixels (32 bytes) -> eight 0565 pixels (16 bytes).
 * R5G6B5 = ((p >> 8)&0xf800) | ((p >> 5)&0x07e0) | ((p >> 3)&0x1f).
 * Correct for the matching RGB or BGR channel order.
 */
    .align 2
    .globl ee_mmi_8888_to_0565_quads
    .ent ee_mmi_8888_to_0565_quads
ee_mmi_8888_to_0565_quads:
    beqz    $a2, 2f
     nop
    lq      $v0, 0($a3)
    lq      $v1, 16($a3)
    lq      $25, 32($a3)
1:
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
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_8888_to_0565_quads
