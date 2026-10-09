/* PS2 R5900 experimental ADD/COPY/FILL 128-bit quads.
 * A: existing four-quad unroll, B: two quads, C: eight quads.
 * Valid for o32 and n32. Inputs/dst aligned 16B, nonoverlapping for COPY.
 * Nonzero count blocks only; no LQ/SQ on zero. Volatile GPRs only.
 * Independent loads are grouped before uses to reduce load-use stalls.
 */
    .text
    .set noreorder
    .align 2
    .globl ee_mmi_add_u2_quads
    .ent ee_mmi_add_u2_quads
ee_mmi_add_u2_quads:
    beqz    $a2, 9f
     nop
1:
    sltiu   $9, $a2, 2
    bnez    $9, 2f
     nop
    lq      $10, 0($a0)
    lq      $11, 0($a1)
    lq      $12, 16($a0)
    lq      $13, 16($a1)
    paddub  $10, $10, $11
    paddub  $12, $12, $13
    sq      $10, 0($a0)
    sq      $12, 16($a0)
    addiu   $a0, $a0, 32
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    lq      $10, 0($a0)
    lq      $11, 0($a1)
    paddub  $10, $10, $11
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_add_u2_quads

    .align 2
    .globl ee_mmi_add_u8_quads
    .ent ee_mmi_add_u8_quads
ee_mmi_add_u8_quads:
    beqz    $a2, 9f
     nop
1:
    sltiu   $9, $a2, 8
    bnez    $9, 2f
     nop
    lq      $10, 0($a0)
    lq      $11, 0($a1)
    lq      $12, 16($a0)
    lq      $13, 16($a1)
    lq      $14, 32($a0)
    lq      $15, 32($a1)
    lq      $24, 48($a0)
    lq      $25, 48($a1)
    paddub  $10, $10, $11
    paddub  $12, $12, $13
    paddub  $14, $14, $15
    paddub  $24, $24, $25
    sq      $10, 0($a0)
    sq      $12, 16($a0)
    sq      $14, 32($a0)
    sq      $24, 48($a0)
    lq      $10, 64($a0)
    lq      $11, 64($a1)
    lq      $12, 80($a0)
    lq      $13, 80($a1)
    lq      $14, 96($a0)
    lq      $15, 96($a1)
    lq      $24, 112($a0)
    lq      $25, 112($a1)
    paddub  $10, $10, $11
    paddub  $12, $12, $13
    paddub  $14, $14, $15
    paddub  $24, $24, $25
    sq      $10, 64($a0)
    sq      $12, 80($a0)
    sq      $14, 96($a0)
    sq      $24, 112($a0)
    addiu   $a0, $a0, 128
    addiu   $a1, $a1, 128
    addiu   $a2, $a2, -8
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    lq      $10, 0($a0)
    lq      $11, 0($a1)
    paddub  $10, $10, $11
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_add_u8_quads

    .align 2
    .globl ee_mmi_copy_u2_quads
    .ent ee_mmi_copy_u2_quads
ee_mmi_copy_u2_quads:
    beqz    $a2, 9f
     nop
1:
    sltiu   $9, $a2, 2
    bnez    $9, 2f
     nop
    lq      $10, 0($a1)
    lq      $11, 16($a1)
    sq      $10, 0($a0)
    sq      $11, 16($a0)
    addiu   $a0, $a0, 32
    addiu   $a1, $a1, 32
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    lq      $10, 0($a1)
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_copy_u2_quads

    .align 2
    .globl ee_mmi_copy_u8_quads
    .ent ee_mmi_copy_u8_quads
ee_mmi_copy_u8_quads:
    beqz    $a2, 9f
     nop
1:
    sltiu   $9, $a2, 8
    bnez    $9, 2f
     nop
    lq      $10, 0($a1)
    lq      $11, 16($a1)
    lq      $12, 32($a1)
    lq      $13, 48($a1)
    lq      $14, 64($a1)
    lq      $15, 80($a1)
    lq      $24, 96($a1)
    lq      $25, 112($a1)
    sq      $10, 0($a0)
    sq      $11, 16($a0)
    sq      $12, 32($a0)
    sq      $13, 48($a0)
    sq      $14, 64($a0)
    sq      $15, 80($a0)
    sq      $24, 96($a0)
    sq      $25, 112($a0)
    addiu   $a0, $a0, 128
    addiu   $a1, $a1, 128
    addiu   $a2, $a2, -8
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    lq      $10, 0($a1)
    sq      $10, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a1, $a1, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_copy_u8_quads

    .align 2
    .globl ee_mmi_fill_u2_quads
    .ent ee_mmi_fill_u2_quads
ee_mmi_fill_u2_quads:
    beqz    $a2, 9f
     nop
    lq      $8, 0($a1)
1:
    sltiu   $9, $a2, 2
    bnez    $9, 2f
     nop
    sq      $8, 0($a0)
    sq      $8, 16($a0)
    addiu   $a0, $a0, 32
    addiu   $a2, $a2, -2
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_fill_u2_quads

    .align 2
    .globl ee_mmi_fill_u8_quads
    .ent ee_mmi_fill_u8_quads
ee_mmi_fill_u8_quads:
    beqz    $a2, 9f
     nop
    lq      $8, 0($a1)
1:
    sltiu   $9, $a2, 8
    bnez    $9, 2f
     nop
    sq      $8, 0($a0)
    sq      $8, 16($a0)
    sq      $8, 32($a0)
    sq      $8, 48($a0)
    sq      $8, 64($a0)
    sq      $8, 80($a0)
    sq      $8, 96($a0)
    sq      $8, 112($a0)
    addiu   $a0, $a0, 128
    addiu   $a2, $a2, -8
    b       1b
     nop
2:
    beqz    $a2, 9f
     nop
3:
    sq      $8, 0($a0)
    addiu   $a0, $a0, 16
    addiu   $a2, $a2, -1
    bnez    $a2, 3b
     nop
9:
    jr      $ra
     nop
    .end ee_mmi_fill_u8_quads

