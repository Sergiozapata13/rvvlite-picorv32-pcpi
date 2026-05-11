// =============================================================================
//  vpu_kernels.h — Kernels vectoriales reutilizables
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Funciones inline que encapsulan los patrones vectoriales mas comunes
//  para que cada main_*.c los pueda usar sin reproducirlos.
//
//  Todas las funciones usan vsetvli dinamico — manejan N arbitrario.
// =============================================================================

#ifndef VPU_KERNELS_H
#define VPU_KERNELS_H

#include <stdint.h>
#include "vpu_asm.h"

// ─── Operaciones VALU element-wise ───────────────────────────────────────────
//
//  Patron general:
//    while (rem > 0) {
//      vsetvli a0, rem, e32m1
//      vle32.v v2, (pa)
//      vle32.v v3, (pb)
//      vOP.vv v1, v2, v3
//      vse32.v v1, (po)
//      rem -= vl, pa += vl, pb += vl, po += vl
//    }
//
//  Macro generador para evitar duplicar codigo:

#define VPU_ELEMENTWISE_KERNEL(NAME, ASM_OP)                            \
static inline void NAME(volatile int32_t *a,                            \
                        volatile int32_t *b,                            \
                        volatile int32_t *out,                          \
                        int n) {                                        \
    int rem = n;                                                        \
    volatile int32_t *pa = a;                                           \
    volatile int32_t *pb = b;                                           \
    volatile int32_t *po = out;                                         \
    while (rem > 0) {                                                   \
        int vl;                                                         \
        asm volatile (                                                  \
            "mv a1, %4\n"                                               \
            ASM_VSETVLI_A0_A1_E32M1                                     \
            "mv %0, a0\n"                                               \
            "mv a0, %1\n"                                               \
            ASM_VLE32_V2_A0                                             \
            "mv a1, %2\n"                                               \
            ASM_VLE32_V3_A1                                             \
            ASM_OP                                                      \
            "mv a0, %3\n"                                               \
            ASM_VSE32_V1_A0                                             \
            : "=r"(vl)                                                  \
            : "r"((uint32_t)pa), "r"((uint32_t)pb),                     \
              "r"((uint32_t)po), "r"(rem)                               \
            : "a0", "a1", "memory"                                      \
        );                                                              \
        rem -= vl;                                                      \
        pa  += vl;                                                      \
        pb  += vl;                                                      \
        po  += vl;                                                      \
    }                                                                   \
}

// Generar kernels para cada operacion VALU
VPU_ELEMENTWISE_KERNEL(vk_add_vv, ASM_VADD_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_sub_vv, ASM_VSUB_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_and_vv, ASM_VAND_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_or_vv,  ASM_VOR_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_xor_vv, ASM_VXOR_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_sll_vv, ASM_VSLL_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_srl_vv, ASM_VSRL_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_mul_vv, ASM_VMUL_VV_V1_V2_V3)


// ─── Dot product: a · b (acumulado en v4[0]) ─────────────────────────────────
static inline int32_t vk_dot(volatile int32_t *a, volatile int32_t *b, int n) {
    // Inicializar acumulador v4 = [0,0,0,0]
    asm volatile (
        ASM_INIT_V4_ZERO
        : : : "a0", "memory"
    );

    int rem = n;
    volatile int32_t *pa = a;
    volatile int32_t *pb = b;

    while (rem > 0) {
        int vl;
        asm volatile (
            "mv a1, %2\n"
            ASM_VSETVLI_A0_A1_E32M1
            "mv %0, a0\n"
            "mv a0, %3\n"
            ASM_VLE32_V2_A0
            "mv a1, %4\n"
            ASM_VLE32_V3_A1
            ASM_VMUL_VV_V1_V2_V3
            ASM_VREDSUM_VS_V4_V1
            : "=r"(vl)
            : "r"((uint32_t)pa), "r"(rem),
              "r"((uint32_t)pa), "r"((uint32_t)pb)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        pa  += vl;
        pb  += vl;
    }

    // Leer resultado de v4[0]
    int32_t result;
    asm volatile (
        ASM_VMV_X_S_A0_V4
        "mv %0, a0\n"
        : "=r"(result) : : "a0", "memory"
    );
    return result;
}


// ─── vse32 throughput: escribir N veces el mismo valor ───────────────────────
//  Util para medir el costo puro de vse32 sin influencia de loads
static inline void vk_store_constant(volatile int32_t *out, int32_t value, int n) {
    // vmv.v.x DENTRO del loop, despues de vsetvli (HT-OE4b)
    // Si se hace fuera, csr_vl puede ser 0 post-reset y no llena ningun elemento
    int rem = n;
    volatile int32_t *po = out;
    while (rem > 0) {
        int vl;
        asm volatile (
            "mv a1, %2\n"
            ASM_VSETVLI_A0_A1_E32M1   // vl = min(rem, VLMAX)
            "mv %0, a0\n"             // guardar vl
            "mv a0, %3\n"             // a0 = value
            ASM_VMV_V_X_V1_A0          // v1[0..vl-1] = value
            "mv a0, %1\n"             // a0 = base ptr
            ASM_VSE32_V1_A0            // store v1 a memoria
            : "=r"(vl)
            : "r"((uint32_t)po), "r"(rem), "r"(value)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        po  += vl;
    }
}


// ─── Equivalentes escalares ──────────────────────────────────────────────────
//  Para comparacion en los benchmarks

#define SCALAR_ELEMENTWISE_KERNEL(NAME, OP)                             \
static inline void NAME(volatile int32_t *a,                            \
                        volatile int32_t *b,                            \
                        volatile int32_t *out,                          \
                        int n) {                                        \
    for (int i = 0; i < n; i++) out[i] = a[i] OP b[i];                  \
}

SCALAR_ELEMENTWISE_KERNEL(sk_add, +)
SCALAR_ELEMENTWISE_KERNEL(sk_sub, -)
SCALAR_ELEMENTWISE_KERNEL(sk_and, &)
SCALAR_ELEMENTWISE_KERNEL(sk_or,  |)
SCALAR_ELEMENTWISE_KERNEL(sk_xor, ^)

// Shifts requieren conversion de uint32_t para vsrl
static inline void sk_sll(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)((uint32_t)a[i] << (b[i] & 0x1F));
}
static inline void sk_srl(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)((uint32_t)a[i] >> (b[i] & 0x1F));
}

static inline void sk_mul(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] * b[i];
}

static __attribute__((noinline)) int32_t sk_dot(volatile int32_t *a, volatile int32_t *b, int n) {
    int32_t acc = 0;
    for (int i = 0; i < n; i++) acc += a[i] * b[i];
    return acc;
}

static inline void sk_store_constant(volatile int32_t *out, int32_t value, int n) {
    for (int i = 0; i < n; i++) out[i] = value;
}

// ─── FIR: y[i] = sum(h[k] * x[(i+k)%n], k=0..n-1) ──────────────────────────
// Escalar: doble loop con multiplicacion software
static __attribute__((noinline))
void sk_fir(volatile int32_t *x, volatile int32_t *h,
            volatile int32_t *y, int n) {
    for (int i = 0; i < n; i++) {
        int32_t acc = 0;
        for (int k = 0; k < n; k++)
            acc += h[k] * x[(i + k) % n];
        y[i] = acc;
    }
}

// Vectorial: N dot products usando vk_dot con ventana deslizante en buf[]
// buf debe apuntar a una zona de RAM de n elementos separada de x, h, y
static __attribute__((noinline))
void vk_fir(volatile int32_t *x, volatile int32_t *h,
            volatile int32_t *y, volatile int32_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        // Copiar ventana deslizante x[(i+k)%n] al buffer
        for (int k = 0; k < n; k++)
            buf[k] = x[(i + k) % n];
        // Dot product vectorial: y[i] = h . buf
        y[i] = vk_dot(h, buf, n);
    }
}

// Agregar al final de vpu_kernels.h, antes del #endif

// =============================================================================
//  AXPY: z[i] = alpha * x[i] + y[i]
//  Kernel BLAS Level-1: combina vmul.vv + vadd.vv en un pass
// =============================================================================

// Escalar — noinline para forzar llamada real a __mulsi3
static __attribute__((noinline))
void sk_axpy(volatile int32_t *x, volatile int32_t *y,
             volatile int32_t *z, int32_t alpha, int n) {
    for (int i = 0; i < n; i++)
        z[i] = alpha * x[i] + y[i];
}

// Vectorial — vmv.v.x (broadcast) + vle32 (x2) + vmul.vv + vadd.vv + vse32
// Encodings derivados:
//   0x5E055157 = vmv.v.x v2, a0   (broadcast alpha a v2)
//   0x02056187 = vle32.v v3, (a0) (carga v3 con base a0)
//   0x9621A0D7 = vmul.vv v1, v2, v3 (v1 = alpha * x)
//   0x021180D7 = vadd.vv v1, v1, v3 (v1 += y)
//   0x020560A7 = vse32.v v1, (a0)  (almacena resultado)
static __attribute__((noinline))
void vk_axpy(volatile int32_t *x, volatile int32_t *y,
             volatile int32_t *z, int32_t alpha, int n) {
    int rem = n;
    volatile int32_t *px = x;
    volatile int32_t *py = y;
    volatile int32_t *pz = z;

    while (rem > 0) {
        int vl;
        asm volatile (
            "mv a1, %1\n"
            ASM_VSETVLI_A0_A1_E32M1     // a0 = vl = min(rem, VLMAX)
            "mv %0, a0\n"                // guardar vl
            "mv a0, %2\n"                // a0 = alpha
            ASM_VMV_V_X_V2_A0            // v2 = [alpha, alpha, alpha, alpha]
            "mv a1, %3\n"                // a1 = px
            ASM_VLE32_V3_A1              // v3 = x[i..i+vl-1]
            ASM_VMUL_VV_V1_V2_V3         // v1 = alpha * x
            "mv a1, %4\n"                // a1 = py
            ASM_VLE32_V3_A1              // v3 = y[i..i+vl-1]  (reusa v3)
            //".word 0x021180D7\n"         // vadd.vv v1, v1, v3 — v1 = alpha*x + y
            ASM_VADD_VV_V1_V1_V3         // v1 += y (acumulador)
            "mv a0, %5\n"                // a0 = pz
            ASM_VSE32_V1_A0              // store v1
            : "=r"(vl)
            : "r"(rem), "r"((int32_t)alpha),
              "r"((uint32_t)px), "r"((uint32_t)py), "r"((uint32_t)pz)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        px  += vl;
        py  += vl;
        pz  += vl;
    }
}

// AXPY extendido escalar: z[i] = a*x[i] + b*y[i] + c*w[i] + d*v[i]
// 4 llamadas a __mulsi3 por elemento — maximiza el costo escalar
static __attribute__((noinline))
void sk_axpy_ext(volatile int32_t *x, volatile int32_t *y,
                 volatile int32_t *w, volatile int32_t *v,
                 volatile int32_t *z,
                 int32_t a, int32_t b, int32_t c, int32_t d, int n) {
    for (int i = 0; i < n; i++)
        z[i] = a*x[i] + b*y[i] + c*w[i] + d*v[i];
}

// AXPY extendido vectorial: 17 instrucciones PCPI por iteracion de 4 elementos
//   4x vmv.v.x + 4x vle32.v + 4x vmul.vv + 3x vadd.vv + 1x vse32.v
//
// Nuevos encodings:
//   0x9621A157 = vmul.vv v2, v2, v3  (acumula termino en v2)
//   0x021100D7 = vadd.vv v1, v1, v2  (suma v2 al acumulador v1)
static __attribute__((noinline))
void vk_axpy_ext(volatile int32_t *x, volatile int32_t *y,
                 volatile int32_t *w, volatile int32_t *v,
                 volatile int32_t *z,
                 int32_t a, int32_t b, int32_t c, int32_t d, int n) {
    int rem = n;
    volatile int32_t *px = x;
    volatile int32_t *py = y;
    volatile int32_t *pw = w;
    volatile int32_t *pv = v;
    volatile int32_t *pz = z;

    while (rem > 0) {
        int vl;
        asm volatile (
            // Configurar vl
            "mv a1, %1\n"
            ASM_VSETVLI_A0_A1_E32M1      // a0 = vl
            "mv %0, a0\n"                 // guardar vl

            // Termino 1: v1 = a * x
            "mv a0, %2\n"                 // a0 = alpha_a
            ASM_VMV_V_X_V2_A0             // v2 = [a,a,a,a]
            "mv a1, %6\n"                 // a1 = px
            ASM_VLE32_V3_A1               // v3 = x[i..i+vl-1]
            ASM_VMUL_VV_V1_V2_V3          // v1 = a * x

            // Termino 2: v2 = b * y  →  v1 += v2
            "mv a0, %3\n"                 // a0 = alpha_b
            ASM_VMV_V_X_V2_A0             // v2 = [b,b,b,b]
            "mv a1, %7\n"                 // a1 = py
            ASM_VLE32_V3_A1               // v3 = y[i..i+vl-1]
            ASM_VMUL_VV_V2_V2_V3
            ASM_VADD_VV_V1_V1_V2

            // Termino 3: v2 = c * w  →  v1 += v2
            "mv a0, %4\n"                 // a0 = alpha_c
            ASM_VMV_V_X_V2_A0             // v2 = [c,c,c,c]
            "mv a1, %8\n"                 // a1 = pw
            ASM_VLE32_V3_A1               // v3 = w[i..i+vl-1]
            ASM_VMUL_VV_V2_V2_V3
            ASM_VADD_VV_V1_V1_V2

            // Termino 4: v2 = d * v  →  v1 += v2
            "mv a0, %5\n"                 // a0 = alpha_d
            ASM_VMV_V_X_V2_A0             // v2 = [d,d,d,d]
            "mv a1, %9\n"                 // a1 = pv
            ASM_VLE32_V3_A1               // v3 = v[i..i+vl-1]
            ASM_VMUL_VV_V2_V2_V3
            ASM_VADD_VV_V1_V1_V2

            // Almacenar resultado
            "mv a0, %10\n"                // a0 = pz
            ASM_VSE32_V1_A0               // store v1

            // vmul.vv v2, v2, v3 = 0x9621A157  (vd=v2, derivado de vmul v1,v2,v3)
            // #define ASM_VMUL_VV_V2_V2_V3   ".word 0x9621A157\n"
            // vadd.vv v1, v1, v2 = 0x021100D7  (vs1=v2, derivado de vadd v1,v1,v3)
            // #define ASM_VADD_VV_V1_V1_V2   ".word 0x021100D7\n"

            : "=r"(vl)
            : "r"(rem),
              "r"((int32_t)a), "r"((int32_t)b),
              "r"((int32_t)c), "r"((int32_t)d),
              "r"((uint32_t)px), "r"((uint32_t)py),
              "r"((uint32_t)pw), "r"((uint32_t)pv),
              "r"((uint32_t)pz)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        px  += vl;  py  += vl;
        pw  += vl;  pv  += vl;
        pz  += vl;
    }
}

#endif // VPU_KERNELS_H
