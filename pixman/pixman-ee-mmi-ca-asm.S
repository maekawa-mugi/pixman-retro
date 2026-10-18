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
    lq      $8, 0($a1)       /* source scaled by CA mask */
    lq      $11, 16($a1)      /* 255 - src_alpha * mask / 255 */
    lq      $9, 0($a0)       /* destination pixels */
    pextlb  $13, $zero, $9  /* first eight destination bytes */
    pextlb  $14, $zero, $11  /* first eight inverse bytes */
    pmulth  $15, $13, $14
    addiu   $a1, $a1, 32
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $25, $15, 8
    pextub  $13, $zero, $9  /* remaining eight destination bytes */
    pextub  $14, $zero, $11  /* remaining eight inverse bytes */
    pmulth  $15, $13, $14
    addiu   $a2, $a2, -1
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    ppacb   $15, $15, $25
    paddub  $8, $8, $15
    sq      $8, 0($a0)
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
    lq      $8, 0($a3)     /* 4 packed solid colors */
    lq      $v1, 16($a3)    /* 8 x 128 */
    lq      $v0, 32($a3)    /* 8 x source alpha */
    lq      $12, 48($a3)    /* 8 x 255 */
1:
    lq      $9, 0($a1)     /* four component-alpha masks */
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

    pmulth  $14, $13, $v0
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $14, $14, 8
    pxor    $14, $14, $12  /* inverse alpha per component */

    pextlb  $11, $zero, $10
    pmulth  $15, $11, $14
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $24, $15, 8
    padduh  $15, $15, $24
    psrlh   $15, $15, 8
    padduh  $25, $25, $15
    pminh   $25, $25, $12  /* saturate to 255 before pack */

    /* Upper eight byte channels: pixels 2 and 3. */
    pextub  $11, $zero, $8
    pextub  $13, $zero, $9
    pmulth  $14, $11, $13
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $24, $14, 8

    pmulth  $14, $13, $v0
    pmfhl.lh $14
    padduh  $14, $14, $v1
    psrlh   $15, $14, 8
    padduh  $14, $14, $15
    psrlh   $14, $14, 8
    pxor    $14, $14, $12

    pextub  $11, $zero, $10
    pmulth  $15, $11, $14
    pmfhl.lh $15
    padduh  $15, $15, $v1
    psrlh   $14, $15, 8
    padduh  $15, $15, $14
    psrlh   $15, $15, 8
    padduh  $24, $24, $15
    pminh   $24, $24, $12

    ppacb   $24, $24, $25
    sq      $24, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_ca_over_solid_quads
