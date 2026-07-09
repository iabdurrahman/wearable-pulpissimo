#ifndef __L3G4200D_SETJMP_H__
#define __L3G4200D_SETJMP_H__

/* jmp_buf minimal untuk RV32: simpan ra, sp, s0-s11 (14 register x 4 byte) */
typedef unsigned int l3g4200d_jmp_buf[14];

static inline int l3g4200d_setjmp(l3g4200d_jmp_buf env)
{
    int ret;
    __asm__ volatile (
        "sw ra,  0(%1)\n"
        "sw sp,  4(%1)\n"
        "sw s0,  8(%1)\n"
        "sw s1, 12(%1)\n"
        "sw s2, 16(%1)\n"
        "sw s3, 20(%1)\n"
        "sw s4, 24(%1)\n"
        "sw s5, 28(%1)\n"
        "sw s6, 32(%1)\n"
        "sw s7, 36(%1)\n"
        "sw s8, 40(%1)\n"
        "sw s9, 44(%1)\n"
        "sw s10,48(%1)\n"
        "sw s11,52(%1)\n"
        "li %0, 0\n"
        : "=r" (ret)
        : "r" (env)
        : "memory"
    );
    return ret;
}

static inline void l3g4200d_longjmp(l3g4200d_jmp_buf env, int val)
{
    __asm__ volatile (
        "lw ra,  0(%0)\n"
        "lw sp,  4(%0)\n"
        "lw s0,  8(%0)\n"
        "lw s1, 12(%0)\n"
        "lw s2, 16(%0)\n"
        "lw s3, 20(%0)\n"
        "lw s4, 24(%0)\n"
        "lw s5, 28(%0)\n"
        "lw s6, 32(%0)\n"
        "lw s7, 36(%0)\n"
        "lw s8, 40(%0)\n"
        "lw s9, 44(%0)\n"
        "lw s10,48(%0)\n"
        "lw s11,52(%0)\n"
        "mv a0, %1\n"
        "ret\n"
        :
        : "r" (env), "r" (val ? val : 1)
        : "memory"
    );
}

#endif