/* PS2 EE fused image+A8 OVER; 4 pixels per 128-bit quad.
 * o32: a0=dst, a1=src, a2=A8 mask, a3=quad count,
 * fifth argument 16(sp)=16-byte aligned [0xff halfwords,0x80 halfwords].
 * C guarantees aligned src/dst and nonoverlapping dest/source/mask.
 */
    .text
    .set noreorder

    .align 2
    .globl ee_mmi_over_image_a8_quads
    .ent ee_mmi_over_image_a8_quads
ee_mmi_over_image_a8_quads:
    beqz    $a3, 2f
     nop
    lw      $t9, 16($sp)   /* o32 fifth pointer parameter */
    lq      $v0, 0($t9)    /* 8 x 255 */
    lq      $v1, 16($t9)   /* 8 x 128 */
1:
    lq      $t0, 0($a1)    /* four premultiplied source pixels */
    psrlw   $t4, $t0, 24  /* 4 source alpha values */
    /* Load exactly four bytes; no alignment or overread assumptions. */
    lbu     $t3, 0($a2)
    lbu     $t5, 1($a2)
    sll     $t5, $t5, 8
    or      $t3, $t3, $t5
    lbu     $t5, 2($a2)
    sll     $t5, $t5, 16
    or      $t3, $t3, $t5
    lbu     $t5, 3($a2)
    sll     $t5, $t5, 24
    or      $t3, $t3, $t5

    /* Four 8-bit masks -> four 16-bit lanes, then duplicate to pairs.
     * t1 = [m0,m0,m1,m1,m2,m2,m3,m3] as eight halfwords.
     */
    pextlb  $t5, $zero, $t3
    pextlh  $t1, $t5, $t5
    lq      $t2, 0($a0)     /* four destination pixels */

    /* Lower eight byte channels: pixels 0 and 1. */
    pextlb  $t3, $zero, $t0
    pextlw  $t5, $t1, $t1   /* mask0 x4, mask1 x4 */
    pmulth  $t6, $t3, $t5
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t9, $t6, 8     /* scaled source */

    pextlw  $t6, $t4, $t4
    psllw   $t7, $t6, 16
    por     $t6, $t6, $t7
    pmulth  $t6, $t5, $t6
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t6, $t6, 8
    pxor    $t6, $t6, $v0  /* inverse alpha per component */

    pextlb  $t3, $zero, $t2
    pmulth  $t7, $t3, $t6
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t7, $t7, 8
    padduh  $t9, $t9, $t7
    pminh   $t9, $t9, $v0  /* saturate to 255 before pack */

    /* Upper eight byte channels: pixels 2 and 3. */
    pextub  $t3, $zero, $t0
    pextuw  $t5, $t1, $t1   /* mask2 x4, mask3 x4 */
    pmulth  $t6, $t3, $t5
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t8, $t6, 8

    pextuw  $t6, $t4, $t4
    psllw   $t7, $t6, 16
    por     $t6, $t6, $t7
    pmulth  $t6, $t5, $t6
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t6, $t6, 8
    pxor    $t6, $t6, $v0

    pextub  $t3, $zero, $t2
    pmulth  $t7, $t3, $t6
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t6, $t7, 8
    padduh  $t7, $t7, $t6
    psrlh   $t7, $t7, 8
    padduh  $t8, $t8, $t7
    pminh   $t8, $t8, $v0

    ppacb   $t8, $t8, $t9
    sq      $t8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, 4
    addiu   $a3, $a3, -1
    bnez    $a3, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_image_a8_quads
