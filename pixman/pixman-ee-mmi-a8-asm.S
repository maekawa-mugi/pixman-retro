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
    lq      $8, 0($a3)     /* 4 packed solid colors */
    lq      $v1, 16($a3)    /* 8 x 128 */
    lq      $v0, 32($a3)    /* 8 x source alpha */
    lq      $12, 48($a3)    /* 8 x 255 */
1:
    /* Load exactly four bytes; no alignment or overread assumptions. */
    lbu     $11, 0($a1)
    lbu     $13, 1($a1)
    sll     $13, $13, 8
    or      $11, $11, $13
    lbu     $13, 2($a1)
    sll     $13, $13, 16
    or      $11, $11, $13
    lbu     $13, 3($a1)
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
    pextuw  $13, $9, $9   /* mask2 x4, mask3 x4 */
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
    addiu   $a1, $a1, 4
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_over_solid_a8_quads
