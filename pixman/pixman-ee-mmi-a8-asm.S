/* PlayStation 2 EE 128-bit MMI: solid + A8 coverage OVER.
 * This object is separate from the general 8888 OVER / component-alpha
 * kernels, so their dispatch and calling conventions remain unchanged.
 */
    .text
    .set noreorder

/* Solid + A8 mask, four pixels per MMI quad.
 * Replicate four individual coverage bytes to all four color channels using
 * PEXTLB -> PEXTLH -> PEXTLW/PEXTUW. No 256-entry LUT or source staging.
 * a0: aligned destination, a1: arbitrary-aligned A8 mask, a2: quads,
 * a3: aligned 64-byte constants (solid, rounding, alpha, 255).
 * Source/mask aliasing is rejected by the C caller. Every mask load is LBU;
 * destination LQ/SQ is always 16-byte aligned. Caller-saved GPRs only.
 */
    .align 2
    .globl ee_mmi_over_solid_a8_quads
    .ent ee_mmi_over_solid_a8_quads
ee_mmi_over_solid_a8_quads:
    beqz    $a2, 2f
     nop
    lq      $t0, 0($a3)     /* 4 packed solid colors */
    lq      $v1, 16($a3)    /* 8 x 128 */
    lq      $v0, 32($a3)    /* 8 x source alpha */
    lq      $t4, 48($a3)    /* 8 x 255 */
1:
    /* Load exactly four bytes; no alignment or overread assumptions. */
    lbu     $t3, 0($a1)
    lbu     $t5, 1($a1)
    sll     $t5, $t5, 8
    or      $t3, $t3, $t5
    lbu     $t5, 2($a1)
    sll     $t5, $t5, 16
    or      $t3, $t3, $t5
    lbu     $t5, 3($a1)
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
    pextuw  $t5, $t1, $t1   /* mask2 x4, mask3 x4 */
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
    addiu   $a1, $a1, 4
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_solid_a8_quads
