/* PS2 EE image + component-alpha OVER: 4 pixels per MMI quad.
 * o32: a0=dst, a1=src, a2=CA mask (all 16-byte aligned),
 * a3=quads, 5th argument at 16(sp) = [255, 128] halfword vectors.
 */
    .text
    .set noreorder
    .align 2
    .globl ee_mmi_over_image_ca_quads
    .ent ee_mmi_over_image_ca_quads
ee_mmi_over_image_ca_quads:
    beqz    $a3, 2f
     nop
    #if _MIPS_SIM == _ABIN32
    move    $25, $8 /* n32 fifth argument */
    #else
    lw      $25, 16($sp)
    #endif
    lq      $v0, 0($25)    /* 8 x 255 */
    lq      $v1, 16($25)   /* 8 x 128 */
1:
    lq      $8, 0($a1)    /* dynamic premultiplied source */
    psrlw   $12, $8, 24  /* independent source alphas */
    lq      $9, 0($a2)    /* four component-alpha masks */
    lq      $10, 0($a0)     /* four destination pixels */

    /* Lower eight byte channels: pixels 0 and 1. */
    pextlb  $11, $zero, $8
    pextlb  $13, $zero, $9
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
    pextub  $13, $zero, $9
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
    addiu   $a2, $a2, 16
    addiu   $a3, $a3, -1
    bnez    $a3, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_image_ca_quads
