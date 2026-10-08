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
    lq      $t0, 0($a1)       /* source pixels 0 .. 3 */
    lq      $t1, 0($a0)       /* destination pixels 0 .. 3 */
    psrlw   $t2, $t0, 24      /* alpha per 32-bit pixel */
    pextlw  $t3, $t2, $t2     /* repeat alpha words of pixel 0/1 */
    pextuw  $t4, $t2, $t2     /* repeat alpha words of pixel 2/3 */
    psllw   $t8, $t3, 16
    por     $t3, $t3, $t8     /* repeat alpha to each 16-bit lane */
    pxor    $t3, $t3, $v0     /* inverse alpha, pixels 0/1 */
    psllw   $t8, $t4, 16
    por     $t4, $t4, $t8
    pxor    $t4, $t4, $v0     /* inverse alpha, pixels 2/3 */
    pextlb  $t5, $zero, $t1   /* zero-extend first 8 destination bytes */
    pextub  $t6, $zero, $t1   /* zero-extend last 8 destination bytes */

    pmulth  $t7, $t5, $t3
    addiu   $a1, $a1, 16
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t9, $t7, 8      /* two pixels in eight 16-bit lanes */

    pmulth  $t7, $t6, $t4
    addiu   $a2, $a2, -1
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t7, $t7, 8
    ppacb   $t7, $t7, $t9    /* low 64-bit: pixel 0/1, high: 2/3 */
    paddub  $t0, $t0, $t7   /* src + rounded(dst * inv_alpha) */
    sq      $t0, 0($a0)
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
    lq      $t9, 32($a3)
1:
    lq      $t0, 0($a1)
    lq      $t1, 16($a1)
    psrlw   $t2, $t0, 8
    pand    $t2, $t2, $v0
    psrlw   $t3, $t0, 5
    pand    $t3, $t3, $v1
    por     $t2, $t2, $t3
    psrlw   $t3, $t0, 3
    pand    $t3, $t3, $t9
    por     $t2, $t2, $t3

    psrlw   $t4, $t1, 8
    pand    $t4, $t4, $v0
    psrlw   $t5, $t1, 5
    pand    $t5, $t5, $v1
    por     $t4, $t4, $t5
    psrlw   $t5, $t1, 3
    pand    $t5, $t5, $t9
    por     $t4, $t4, $t5

    ppach   $t2, $t4, $t2
    sq      $t2, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_8888_to_0565_quads
