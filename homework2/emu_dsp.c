/*
 * DSP maddr32 emulation for Nuclei xxldsp on QEMU 2024.06
 *
 * Intercepts illegal instruction exceptions (mcause=2).
 * The handler function __dsp_emu_trampoline is written in assembly
 * below to precisely control register save/restore.
 */

#include <stdint.h>

/* Debug counter - visible to verify handler is called */
volatile uint32_t emu_enter_count;
volatile uint32_t emu_last_mepc;
volatile uint32_t emu_last_inst;

/*
 * Word offsets in saved exception context.
 * These MUST match the SAVE_CONTEXT macro in
 * galaxy_sdk/bsp/src/intexc_riscv.S (and portasm.S).
 * If the SDK updates those, these offsets need updating.
 * The restore order in exc_entry is: msubm(52), mepc(48), mcause(44).
 */
#define CTX_RA   0
#define CTX_TP   1
#define CTX_T0   2
#define CTX_T1   3
#define CTX_T2   4
#define CTX_A0   5
#define CTX_A1   6
#define CTX_A2   7
#define CTX_A3   8
#define CTX_A4   9
#define CTX_A5  10
#define CTX_MCAUSE 11  /* csrw pushmcause,11 stores at sp+44 */
#define CTX_MEPC   12  /* csrw pushmepc,12   stores at sp+48 */
#define CTX_MSUBM  13  /* csrw pushmsubm,13  stores at sp+52 */
#define CTX_A6  14
#define CTX_A7  15
#define CTX_T3  16
#define CTX_T4  17
#define CTX_T5  18
#define CTX_T6  19

static uint32_t ctx_get(uint32_t *ctx, int r)
{
    switch (r) {
    case 1: return ctx[CTX_RA];   case 4: return ctx[CTX_TP];
    case 5: return ctx[CTX_T0];   case 6: return ctx[CTX_T1];
    case 7: return ctx[CTX_T2];   case 10: return ctx[CTX_A0];
    case 11: return ctx[CTX_A1];  case 12: return ctx[CTX_A2];
    case 13: return ctx[CTX_A3];  case 14: return ctx[CTX_A4];
    case 15: return ctx[CTX_A5];  case 16: return ctx[CTX_A6];
    case 17: return ctx[CTX_A7];  case 28: return ctx[CTX_T3];
    case 29: return ctx[CTX_T4];  case 30: return ctx[CTX_T5];
    case 31: return ctx[CTX_T6];  default: return 0;
    }
}

static void ctx_set(uint32_t *ctx, int r, uint32_t v)
{
    switch (r) {
    case 1: ctx[CTX_RA]=v; break;  case 4: ctx[CTX_TP]=v; break;
    case 5: ctx[CTX_T0]=v; break;  case 6: ctx[CTX_T1]=v; break;
    case 7: ctx[CTX_T2]=v; break;  case 10:ctx[CTX_A0]=v; break;
    case 11:ctx[CTX_A1]=v; break;  case 12:ctx[CTX_A2]=v; break;
    case 13:ctx[CTX_A3]=v; break;  case 14:ctx[CTX_A4]=v; break;
    case 15:ctx[CTX_A5]=v; break;  case 16:ctx[CTX_A6]=v; break;
    case 17:ctx[CTX_A7]=v; break;  case 28:ctx[CTX_T3]=v; break;
    case 29:ctx[CTX_T4]=v; break;  case 30:ctx[CTX_T5]=v; break;
    case 31:ctx[CTX_T6]=v; break;  default: break;
    }
}

/* Called from assembly trampoline with snapshotted callee-saved regs.
 * save[0]=s0(x8), save[1]=s1(x9), save[2]=s2(x18)...save[11]=s11(x27)
 * Returns updated save[] for modified callee-saved destinations. */
__attribute__((used))
static void do_emulate(uint32_t *ctx, uint32_t save[12])
{
    uint32_t mepc;
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));

    uint32_t inst = *(uint32_t *)mepc;

    emu_enter_count++;
    emu_last_mepc = mepc;
    emu_last_inst = inst;

    if ((inst & 0x7F) == 0x7F) {
        /*
         * maddr32 variants in custom-3 space (opcode 0x7F, funct7 >= 0x62):
         *
         * funct3=2: rd = rs1*rs2 + rd     (standard R-type: rd/rs1/rs2)
         * funct3=3: rd = rs1*rs2 + rd     (standard R-type)
         * funct3=5: a4 = a4 + rB*rC       (implicit dest a4; rd_field=rB, rs2_field=rC)
         * funct3=6: rd = rs1*rs2 + rd     (standard R-type)
         *
         * For funct3=5, the encoding differs: the standard "rd" field holds
         * the second source operand, and a4(x14) is the implicit accumulator.
         */
        int funct3 = (inst >> 12) & 0x7;

        int rd, rs1, rs2;
        rd  = (inst >> 7)  & 0x1F;
        rs1 = (inst >> 15) & 0x1F;
        rs2 = (inst >> 20) & 0x1F;

        /*
         * Resolve source register values.
         * For funct3=5, rd_field is NOT the destination but the multiply op1.
         * The real destination is a4 (x14).
         */
        uint32_t vr, v1, v2;

        if (funct3 == 5) {
            /* Dest=a4, op1=rd_field, op2=rs2_field.  Accumulator is a4. */
            if (rd >= 8 && rd <= 9)       v1 = save[rd - 8];
            else if (rd >= 18 && rd <= 27) v1 = save[rd - 16];
            else                         v1 = ctx_get(ctx, rd);

            if (rs2 >= 8 && rs2 <= 9)       v2 = save[rs2 - 8];
            else if (rs2 >= 18 && rs2 <= 27) v2 = save[rs2 - 16];
            else                           v2 = ctx_get(ctx, rs2);

            if (14 >= 8 && 14 <= 9)       vr = save[0];   /* a4=x14 */
            else if (14 >= 18 && 14 <= 27) vr = save[14-16];
            else                         vr = ctx_get(ctx, 14);

            uint32_t result = v1 * v2 + vr;
            /* Compiler barrier: prevent dead-code elimination of the
             * store below.  GCC may not see that ctx is read after
             * this function returns (in the exc_entry epilogue). */
            __asm__ volatile("" ::: "memory");

            ctx_set(ctx, 14, result);
        } else {
            /* Standard multiply-accumulate: rd = rs1*rs2 + rd (funct3=2,3,6) */
            if (rd == 8)       vr = save[0];
            else if (rd == 9)  vr = save[1];
            else if (rd >= 18 && rd <= 27) vr = save[rd - 16];
            else               vr = ctx_get(ctx, rd);

            if (rs1 == 8)       v1 = save[0];
            else if (rs1 == 9)  v1 = save[1];
            else if (rs1 >= 18 && rs1 <= 27) v1 = save[rs1 - 16];
            else                v1 = ctx_get(ctx, rs1);

            if (rs2 == 8)       v2 = save[0];
            else if (rs2 == 9)  v2 = save[1];
            else if (rs2 >= 18 && rs2 <= 27) v2 = save[rs2 - 16];
            else                v2 = ctx_get(ctx, rs2);

            uint32_t result = v1 * v2 + vr;
            __asm__ volatile("" ::: "memory");

            if (rd == 8)       save[0] = result;
            else if (rd == 9)  save[1] = result;
            else if (rd >= 18 && rd <= 27) save[rd - 16] = result;
            else               ctx_set(ctx, rd, result);
        }
    }

    /* Forward MEPC past this instruction */
    ctx[CTX_MEPC] = mepc + 4;
}

/*
 * Assembly trampoline.  Called by core_exception_handler with:
 *   a0 = mcause
 *   a1 = ctx (exception stack pointer)
 *
 * We save callee-saved regs, call do_emulate(), restore callee-saved
 * regs, and return.  GCC's C epilogue does NOT run, so our asm
 * register writes are not overwritten.
 */
__asm__(
    ".section .text\n"
    ".global dsp_emu_illegal_handler\n"
    ".type dsp_emu_illegal_handler, @function\n"
    "dsp_emu_illegal_handler:\n"
    "    addi sp, sp, -64\n"
    "    sw   s0,  0(sp)\n"
    "    sw   s1,  4(sp)\n"
    "    sw   s2,  8(sp)\n"
    "    sw   s3,  12(sp)\n"
    "    sw   s4,  16(sp)\n"
    "    sw   s5,  20(sp)\n"
    "    sw   s6,  24(sp)\n"
    "    sw   s7,  28(sp)\n"
    "    sw   s8,  32(sp)\n"
    "    sw   s9,  36(sp)\n"
    "    sw   s10, 40(sp)\n"
    "    sw   s11, 44(sp)\n"
    "    sw   ra,  48(sp)\n"
    "    mv   a2, a1\n"               /* a2 = ctx */
    "    mv   a1, sp\n"               /* a1 = save[] array on stack */
    "    mv   a0, a2\n"               /* a0 = ctx */
    "    call do_emulate\n"
    "    lw   s0,  0(sp)\n"
    "    lw   s1,  4(sp)\n"
    "    lw   s2,  8(sp)\n"
    "    lw   s3,  12(sp)\n"
    "    lw   s4,  16(sp)\n"
    "    lw   s5,  20(sp)\n"
    "    lw   s6,  24(sp)\n"
    "    lw   s7,  28(sp)\n"
    "    lw   s8,  32(sp)\n"
    "    lw   s9,  36(sp)\n"
    "    lw   s10, 40(sp)\n"
    "    lw   s11, 44(sp)\n"
    "    lw   ra,  48(sp)\n"
    "    addi sp, sp, 64\n"
    "    li   a0, 0\n"
    "    ret\n"
);
