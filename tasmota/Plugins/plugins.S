
    .section .text.mod_part
    .align 4
    .global j_mulsf3
    .type   j_mulsf3,@function
    j_mulsf3:
    addi	sp, sp, -8
    s32i.n	a4, sp, 4
    l32r	a4, module_header+52
    l32i	a4, a4, 160 # table offset of mulsf3

    # common part
    s32i.n	a0, sp, 0
    callx0 a4
    l32i.n	a0, sp, 0
    l32i.n	a4, sp, 4
    addi	sp, sp, 8
    ret.n


    .section .text.mod_part
    .align 4
    .global gettbl
    .type   gettbl,@function
    gettbl:
    l32r	a2, module_header+48
    ret.n


