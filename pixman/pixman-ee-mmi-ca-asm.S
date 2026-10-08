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
