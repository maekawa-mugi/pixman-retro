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
    #if _MIPS_SIM == _ABIN32
    move    $25, $8 /* n32 fifth argument */
    #else
    lw      $25, 16($sp)   /* o32 fifth pointer parameter */
    #endif
    lq      $v0, 0($25)    /* 8 x 255 */
    lq      $v1, 16($25)   /* 8 x 128 */
1:
    lq      $8, 0($a1)    /* four premultiplied source pixels */
    psrlw   $12, $8, 24  /* 4 source alpha values */
    /* Load exactly four bytes; no alignment or overread assumptions. */
    lbu     $11, 0($a2)
    lbu     $13, 1($a2)
    sll     $13, $13, 8
    or      $11, $11, $13
    lbu     $13, 2($a2)
    sll     $13, $13, 16
    or      $11, $11, $13
    lbu     $13, 3($a2)
    sll     $13, $13, 24
    or      $11, $11, $13

    /* Four 8-bit masks -> four 16-bit lanes, then duplicate to pairs.
     * t1 = [m0,m0,m1,m1,m2,m2,m3,m3] as eight halfwords.
     */
    pextlb  $13, $zero, $11
    pextlh  $9, $13, $13
    lq      $10, 0($a0)     /* four destination pixels */

    /* Lower eight byte channels: pixels 0 and 1. */
    pextlb  $11, $zero, $8
    pextlw  $13, $9, $9   /* mask0 x4, mask1 x4 */
    pmulth  $14, $11, $13
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $25, $14, 8     /* scaled source */

    pextlw  $14, $12, $12
    psllw   $15, $14, 16
    por     $14, $14, $15
    pmulth  $14, $13, $14
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $14, $14, 8
    pxor    $14, $14, $v0  /* inverse alpha per component */

    pextlb  $11, $zero, $10
    pmulth  $15, $11, $14
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    padduh  $25, $25, $15
    pminh   $25, $25, $v0  /* saturate to 255 before pack */

    /* Upper eight byte channels: pixels 2 and 3. */
    pextub  $11, $zero, $8
    pextuw  $13, $9, $9   /* mask2 x4, mask3 x4 */
    pmulth  $14, $11, $13
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $24, $14, 8

    pextuw  $14, $12, $12
    psllw   $15, $14, 16
    por     $14, $14, $15
    pmulth  $14, $13, $14
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $14, $14, 8
    pxor    $14, $14, $v0

    pextub  $11, $zero, $10
    pmulth  $15, $11, $14
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $14, $15, 8
    padduh  $15, $15, $14
    psrlh   $15, $15, 8
    padduh  $24, $24, $15
    pminh   $24, $24, $v0

    ppacb   $24, $24, $25
    sq      $24, 0($a0)
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
