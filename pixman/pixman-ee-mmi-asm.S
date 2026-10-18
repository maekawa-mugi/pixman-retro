/*
 * PS2 Emotion Engine R5900 128-bit MMI scanline kernels.
 * o32 ABI; arguments are a0-a2 (a3 for A8 IN).
 * Only caller-saved GPRs are used, including t0-t8 and v1.
 * All LQ/SQ operands are 16-byte aligned. Zero-count calls are safe.
 */
    .text
    .set    noreorder

    .align  2
    .globl  ee_mmi_add_quads
    .ent    ee_mmi_add_quads
ee_mmi_add_quads:
    beqz    $a2, 4f
     nop
    sltiu   $10, $a2, 4
    bnez    $10, 2f
     nop
1:
    /* Four independent aligned 16-byte blocks per iteration. */
    lq      $8, 0($a0)
    lq      $9, 0($a1)
    lq      $11, 16($a0)
    lq      $12, 16($a1)
    paddub  $8, $8, $9
    lq      $13, 32($a0)
    lq      $14, 32($a1)
    paddub  $11, $11, $12
    lq      $15, 48($a0)
    lq      $24, 48($a1)
    paddub  $13, $13, $14
    sq      $8, 0($a0)
    paddub  $15, $15, $24
    sq      $11, 16($a0)
    sq      $13, 32($a0)
    sq      $15, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a1, $a1, 64
    addiu   $a2, $a2, -4
    sltiu   $10, $a2, 4
    beqz    $10, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    lq      $8, 0($a0)
    lq      $9, 0($a1)
    paddub  $8, $8, $9
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
4:
    jr      $ra
     nop
    .end    ee_mmi_add_quads

    .align  2
    .globl  ee_mmi_copy_quads
    .ent    ee_mmi_copy_quads
ee_mmi_copy_quads:
    beqz    $a2, 4f
     nop
    sltiu   $10, $a2, 4
    bnez    $10, 2f
     nop
1:
    /* The C caller guarantees nonoverlapping, aligned spans. */
    lq      $8, 0($a1)
    lq      $9, 16($a1)
    lq      $11, 32($a1)
    lq      $12, 48($a1)
    sq      $8, 0($a0)
    sq      $9, 16($a0)
    sq      $11, 32($a0)
    sq      $12, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a1, $a1, 64
    addiu   $a2, $a2, -4
    sltiu   $10, $a2, 4
    beqz    $10, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    lq      $8, 0($a1)
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
4:
    jr      $ra
     nop
    .end    ee_mmi_copy_quads

    .align  2
    .globl  ee_mmi_fill_quads
    .ent    ee_mmi_fill_quads
ee_mmi_fill_quads:
    beqz    $a2, 4f
     nop
    lq      $8, 0($a1)
    sltiu   $9, $a2, 4
    bnez    $9, 2f
     nop
1:
    sq      $8, 0($a0)
    sq      $8, 16($a0)
    sq      $8, 32($a0)
    sq      $8, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a2, $a2, -4
    sltiu   $9, $a2, 4
    beqz    $9, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
4:
    jr      $ra
     nop
    .end    ee_mmi_fill_quads


/* A8 IN blending, 16 lanes: dst[i] = round(src[i] * dst[i] / 255).
 * The two 128-bit input pointers are aligned on entry.
 * a3 points to eight aligned 0x0080 halfword constants.
 */
    .align 2
    .globl ee_mmi_in_a8_quads
    .ent ee_mmi_in_a8_quads
ee_mmi_in_a8_quads:
    beqz    $a2, 2f
     nop
    lq      $v1, 0($a3)
1:
    lq      $8, 0($a1)
    lq      $9, 0($a0)
    pextlb  $10, $zero, $8
    pextlb  $11, $zero, $9
    pmulth  $12, $10, $11
    addiu   $a1, $a1, 16
    pmfhl.lh $12
    padduh  $12, $12, $v1
    psrlh   $13, $12, 8
    padduh  $12, $12, $13
    psrlh   $14, $12, 8
    pextub  $10, $zero, $8
    pextub  $11, $zero, $9
    pmulth  $12, $10, $11
    addiu   $a2, $a2, -1
    pmfhl.lh $12
    padduh  $12, $12, $v1
    psrlh   $13, $12, 8
    padduh  $12, $12, $13
    psrlh   $15, $12, 8
    ppacb   $15, $15, $14
    sq      $15, 0($a0)
    addiu   $a0, $a0, 16
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_in_a8_quads
