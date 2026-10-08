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
    sltiu   $t2, $a2, 4
    bnez    $t2, 2f
     nop
1:
    /* Four independent aligned 16-byte blocks per iteration. */
    lq      $t0, 0($a0)
    lq      $t1, 0($a1)
    lq      $t3, 16($a0)
    lq      $t4, 16($a1)
    paddub  $t0, $t0, $t1
    lq      $t5, 32($a0)
    lq      $t6, 32($a1)
    paddub  $t3, $t3, $t4
    lq      $t7, 48($a0)
    lq      $t8, 48($a1)
    paddub  $t5, $t5, $t6
    sq      $t0, 0($a0)
    paddub  $t7, $t7, $t8
    sq      $t3, 16($a0)
    sq      $t5, 32($a0)
    sq      $t7, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a1, $a1, 64
    addiu   $a2, $a2, -4
    sltiu   $t2, $a2, 4
    beqz    $t2, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    lq      $t0, 0($a0)
    lq      $t1, 0($a1)
    paddub  $t0, $t0, $t1
    sq      $t0, 0($a0)
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
    sltiu   $t2, $a2, 4
    bnez    $t2, 2f
     nop
1:
    /* The C caller guarantees nonoverlapping, aligned spans. */
    lq      $t0, 0($a1)
    lq      $t1, 16($a1)
    lq      $t3, 32($a1)
    lq      $t4, 48($a1)
    sq      $t0, 0($a0)
    sq      $t1, 16($a0)
    sq      $t3, 32($a0)
    sq      $t4, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a1, $a1, 64
    addiu   $a2, $a2, -4
    sltiu   $t2, $a2, 4
    beqz    $t2, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    lq      $t0, 0($a1)
    sq      $t0, 0($a0)
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
    lq      $t0, 0($a1)
    sltiu   $t1, $a2, 4
    bnez    $t1, 2f
     nop
1:
    sq      $t0, 0($a0)
    sq      $t0, 16($a0)
    sq      $t0, 32($a0)
    sq      $t0, 48($a0)
    addiu   $a0, $a0, 64
    addiu   $a2, $a2, -4
    sltiu   $t1, $a2, 4
    beqz    $t1, 1b
     nop
2:
    beqz    $a2, 4f
     nop
3:
    sq      $t0, 0($a0)
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
    lq      $t0, 0($a1)
    lq      $t1, 0($a0)
    pextlb  $t2, $zero, $t0
    pextlb  $t3, $zero, $t1
    pmulth  $t4, $t2, $t3
    addiu   $a1, $a1, 16
    pmfhl.lh $t4
    padduh  $t4, $t4, $v1
    psrlh   $t5, $t4, 8
    padduh  $t4, $t4, $t5
    psrlh   $t6, $t4, 8
    pextub  $t2, $zero, $t0
    pextub  $t3, $zero, $t1
    pmulth  $t4, $t2, $t3
    addiu   $a2, $a2, -1
    pmfhl.lh $t4
    padduh  $t4, $t4, $v1
    psrlh   $t5, $t4, 8
    padduh  $t4, $t4, $t5
    psrlh   $t7, $t4, 8
    ppacb   $t7, $t7, $t6
    sq      $t7, 0($a0)
    addiu   $a0, $a0, 16
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end ee_mmi_in_a8_quads
