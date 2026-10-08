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
    lw      $t9, 16($sp)
    lq      $v0, 0($t9)    /* 8 x 255 */
    lq      $v1, 16($t9)   /* 8 x 128 */
1:
    lq      $t0, 0($a1)    /* dynamic premultiplied source */
    psrlw   $t4, $t0, 24  /* independent source alphas */
    lq      $t1, 0($a2)    /* four component-alpha masks */
    lq      $t2, 0($a0)     /* four destination pixels */

    /* Lower eight byte channels: pixels 0 and 1. */
    pextlb  $t3, $zero, $t0
    pextlb  $t5, $zero, $t1
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
    pextub  $t5, $zero, $t1
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
    addiu   $a2, $a2, 16
    addiu   $a3, $a3, -1
    bnez    $a3, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_image_ca_quads
