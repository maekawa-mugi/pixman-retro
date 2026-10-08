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
    lq      $v0, 0($a3)     /* eight 5-bit masks */
    lq      $v1, 16($a3)    /* eight 6-bit masks */
1:
    lq      $t0, 0($a0)     /* eight RGB565 destination pixels */
    lq      $t1, 0($a1)     /* source pixels 0..3 */
    lq      $t2, 16($a1)    /* source pixels 4..7 */

    /* RGB565 little endian: blue 5, green 6, red 5.
     * Convert using bit replication matching convert_0565_to_0888.
     */
    pand    $t3, $t0, $v0
    psllh   $t4, $t3, 3
    psrlh   $t3, $t3, 2
    por     $t4, $t4, $t3  /* B8 */

    psrlh   $t3, $t0, 5
    pand    $t3, $t3, $v1
    psllh   $t5, $t3, 2
    psrlh   $t3, $t3, 4
    por     $t5, $t5, $t3  /* G8 */
    psllh   $t5, $t5, 8
    por     $t4, $t4, $t5  /* 16-bit packed BG */

    psrlh   $t3, $t0, 11
    psllh   $t5, $t3, 3
    psrlh   $t3, $t3, 2
    por     $t5, $t5, $t3  /* R8 */
    pextlh  $t3, $t5, $t4  /* pixels 0..3 in 32-bit BGR0 */
    pextuh  $t4, $t5, $t4  /* pixels 4..7 in 32-bit BGR0 */

    lq      $v0, 32($a3)   /* eight 0x00ff: inverse alpha */
    lq      $v1, 48($a3)   /* eight 0x0080: /255 rounding */

    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $t5, $t1, 24
    pextlw  $t6, $t5, $t5
    pextuw  $t7, $t5, $t5
    psllw   $t8, $t6, 16
    por     $t6, $t6, $t8
    pxor    $t6, $t6, $v0
    psllw   $t8, $t7, 16
    por     $t7, $t7, $t8
    pxor    $t7, $t7, $v0

    pextlb  $t5, $zero, $t3
    pmulth  $t9, $t5, $t6
    pmfhl.lh $t9
    padduh  $t9, $t9, $v1
    psrlh   $t8, $t9, 8
    padduh  $t9, $t9, $t8
    psrlh   $t9, $t9, 8

    pextub  $t5, $zero, $t3
    pmulth  $t8, $t5, $t7
    pmfhl.lh $t8
    padduh  $t8, $t8, $v1
    psrlh   $t5, $t8, 8
    padduh  $t8, $t8, $t5
    psrlh   $t8, $t8, 8
    ppacb   $t8, $t8, $t9
    paddub  $t1, $t1, $t8


    /* Four premultiplied source pixels OVER four expanded 565 pixels. */
    psrlw   $t5, $t2, 24
    pextlw  $t6, $t5, $t5
    pextuw  $t7, $t5, $t5
    psllw   $t8, $t6, 16
    por     $t6, $t6, $t8
    pxor    $t6, $t6, $v0
    psllw   $t8, $t7, 16
    por     $t7, $t7, $t8
    pxor    $t7, $t7, $v0

    pextlb  $t5, $zero, $t4
    pmulth  $t9, $t5, $t6
    pmfhl.lh $t9
    padduh  $t9, $t9, $v1
    psrlh   $t8, $t9, 8
    padduh  $t9, $t9, $t8
    psrlh   $t9, $t9, 8

    pextub  $t5, $zero, $t4
    pmulth  $t8, $t5, $t7
    pmfhl.lh $t8
    padduh  $t8, $t8, $v1
    psrlh   $t5, $t8, 8
    padduh  $t8, $t8, $t5
    psrlh   $t8, $t8, 8
    ppacb   $t8, $t8, $t9
    paddub  $t2, $t2, $t8

    /* 8888 -> RGB565: Pixman's truncation, no further rounding.
     * Each 32-bit word now holds a packed 16-bit RGB565 result.
     */
    lq      $v0, 64($a3)   /* 0x0000f800 */
    lq      $v1, 80($a3)   /* 0x000007e0 */
    lq      $t9, 96($a3)   /* 0x0000001f */

    psrlw   $t3, $t1, 8
    pand    $t3, $t3, $v0
    psrlw   $t5, $t1, 5
    pand    $t5, $t5, $v1
    por     $t3, $t3, $t5
    psrlw   $t5, $t1, 3
    pand    $t5, $t5, $t9
    por     $t3, $t3, $t5

    psrlw   $t4, $t2, 8
    pand    $t4, $t4, $v0
    psrlw   $t5, $t2, 5
    pand    $t5, $t5, $v1
    por     $t4, $t4, $t5
    psrlw   $t5, $t2, 3
    pand    $t5, $t5, $t9
    por     $t4, $t4, $t5

    ppach   $t3, $t4, $t3
    sq      $t3, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_8888_0565_quads
