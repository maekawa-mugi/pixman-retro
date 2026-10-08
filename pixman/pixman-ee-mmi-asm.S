/*
 * PS2 Emotion Engine R5900 128-bit MMI scanline kernels.
 * o32 ABI; arguments are a0, a1, a2. Only caller-saved t0/t1 are clobbered.
 * All LQ/SQ operands are 16-byte aligned. Zero-count calls are safe.
 */
    .text
    .set    noreorder

    .align  2
    .globl  ee_mmi_add_quads
    .ent    ee_mmi_add_quads
ee_mmi_add_quads:
    beqz    $a2, 2f
     nop
1:
    lq      $t0, 0($a0)
    lq      $t1, 0($a1)
    paddub  $t0, $t0, $t1
    sq      $t0, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end    ee_mmi_add_quads

    .align  2
    .globl  ee_mmi_copy_quads
    .ent    ee_mmi_copy_quads
ee_mmi_copy_quads:
    beqz    $a2, 2f
     nop
1:
    lq      $t0, 0($a1)
    sq      $t0, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end    ee_mmi_copy_quads

    .align  2
    .globl  ee_mmi_fill_quads
    .ent    ee_mmi_fill_quads
ee_mmi_fill_quads:
    beqz    $a2, 2f
     nop
    lq      $t0, 0($a1)
1:
    sq      $t0, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 1b
     nop
2:
    jr      $ra
     nop
    .end    ee_mmi_fill_quads
