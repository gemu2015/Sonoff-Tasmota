
#ifdef ESP8266
    .section .text.mod_part
    .align 4
    .global gettbl
    .type   gettbl,@function
    gettbl:
    l32r	a2, module_header+48
    ret.n

#endif


#ifdef ESP32
//#if 0
    .section .plugin.mod_part
    .align 4
    .global gettbl
    .type   gettbl,@function
    gettbl:
    entry a1,32
    l32r	a2, module_header+48
    retw.n
#endif



#if 0



risc
FORCE_INLINE_ATTR void *rv_utils_get_sp(void)
{
    void *sp;
    asm volatile ("mv %0, sp;" : "=r" (sp));
    return sp;
}
FORCE_INLINE_ATTR void __attribute__((always_inline)) rv_utils_wait_for_intr(void)
{
    asm volatile ("wfi\n");
}
FORCE_INLINE_ATTR uint32_t __attribute__((always_inline)) rv_utils_get_cycle_count(void)
{
    return RV_READ_CSR(CSR_PCCR_MACHINE);
}

FORCE_INLINE_ATTR void __attribute__((always_inline)) rv_utils_set_cycle_count(uint32_t ccount)
{
    RV_WRITE_CSR(CSR_PCCR_MACHINE, ccount);
}



xtensa
FORCE_INLINE_ATTR void *xt_utils_get_sp(void)
{
    void *sp;
    asm volatile ("mov %0, sp;" : "=r" (sp));
    return sp;
}
FORCE_INLINE_ATTR void xt_utils_set_vecbase(uint32_t vecbase)
{
    asm volatile ("wsr %0, vecbase" :: "r" (vecbase));
}

FORCE_INLINE_ATTR void xt_utils_wait_for_intr(void)
{
    asm volatile ("waiti 0\n");
}

FORCE_INLINE_ATTR uint32_t xt_utils_get_cycle_count(void)
{
    uint32_t ccount;
    RSR(CCOUNT, ccount);
    return ccount;
}

static inline void xt_utils_set_cycle_count(uint32_t ccount)
{
    WSR(CCOUNT, ccount);
}

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

#endif
