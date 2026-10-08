/*
 * PS2 EE component-alpha OVER: src_scaled + dst * inverse / 255.
 * a0=aligned dst (4 pixels), a1=aligned prepared pairs:
 *  [4 source words, 4 inverse-per-channel words]
 * a2=quad count, a3=16-byte aligned halfword rounding constant 128.
 *
 * Each product <=255*255. PMULTH writes GPR and full HI/LO; PMFHL.LH
 * extracts all eight low-halfwords of products. All intermediate 16-bit
 * additions remain below 65536.
 */
    .text
    .set noreorder
    .align 2
    .globl ee_mmi_ca_over_quads
    .ent ee_mmi_ca_over_quads
ee_mmi_ca_over_quads:
    beqz    $a2, 2f
     nop
    lq      $v1, 0($a3)
1:
    lq      $t0, 0($a1)       /* source scaled by CA mask */
    lq      $t3, 16($a1)      /* 255 - src_alpha * mask / 255 */
    lq      $t1, 0($a0)       /* destination pixels */
    pextlb  $t5, $zero, $t1  /* first eight destination bytes */
    pextlb  $t6, $zero, $t3  /* first eight inverse bytes */
    pmulth  $t7, $t5, $t6
    addiu   $a1, $a1, 32
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t9, $t7, 8
    pextub  $t5, $zero, $t1  /* remaining eight destination bytes */
    pextub  $t6, $zero, $t3  /* remaining eight inverse bytes */
    pmulth  $t7, $t5, $t6
    addiu   $a2, $a2, -1
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t7, $t7, 8
    ppacb   $t7, $t7, $t9
    paddub  $t0, $t0, $t7
    sq      $t0, 0($a0)
    addiu   $a0, $a0, 16
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_ca_over_quads

/* Fused solid + component-alpha OVER for four 8888 pixels:
 * s = round(solid * mask / 255)
 * inverse = 255 - round(alpha(solid) * mask / 255)
 * dst = min(255, s + round(dst * inverse / 255))
 * a0=aligned destination, a1=aligned CA mask, a2=quads,
 * a3=16-byte aligned 64-byte constants: solid[4], halfword128[8],
 * halfwordAlpha[8], halfword255[8].
 * Caller-saved GPRs only. Both data pointers must be aligned.
 */
    .align 2
    .globl ee_mmi_ca_over_solid_quads
    .ent ee_mmi_ca_over_solid_quads
ee_mmi_ca_over_solid_quads:
    beqz    $a2, 2f
     nop
    lq      $t0, 0($a3)     /* 4 packed solid colors */
    lq      $v1, 16($a3)    /* 8 x 128 */
    lq      $v0, 32($a3)    /* 8 x source alpha */
    lq      $t4, 48($a3)    /* 8 x 255 */
1:
    lq      $t1, 0($a1)     /* four component-alpha masks */
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

    pmulth  $t6, $t5, $v0
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t6, $t6, 8
    pxor    $t6, $t6, $t4  /* inverse alpha per component */

    pextlb  $t3, $zero, $t2
    pmulth  $t7, $t3, $t6
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t8, $t7, 8
    padduh  $t7, $t7, $t8
    psrlh   $t7, $t7, 8
    padduh  $t9, $t9, $t7
    pminh   $t9, $t9, $t4  /* saturate to 255 before pack */

    /* Upper eight byte channels: pixels 2 and 3. */
    pextub  $t3, $zero, $t0
    pextub  $t5, $zero, $t1
    pmulth  $t6, $t3, $t5
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t8, $t6, 8

    pmulth  $t6, $t5, $v0
    pmfhl.lh $t6
    padduh  $t6, $t6, $v1
    psrlh   $t7, $t6, 8
    padduh  $t6, $t6, $t7
    psrlh   $t6, $t6, 8
    pxor    $t6, $t6, $t4

    pextub  $t3, $zero, $t2
    pmulth  $t7, $t3, $t6
    pmfhl.lh $t7
    padduh  $t7, $t7, $v1
    psrlh   $t6, $t7, 8
    padduh  $t7, $t7, $t6
    psrlh   $t7, $t7, 8
    padduh  $t8, $t8, $t7
    pminh   $t8, $t8, $t4

    ppacb   $t8, $t8, $t9
    sq      $t8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_ca_over_solid_quads
