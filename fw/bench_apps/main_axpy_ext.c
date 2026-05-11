// =============================================================================
//  main_axpy_ext.c — Benchmark AXPY extendido
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Operacion: z[i] = a*x[i] + b*y[i] + c*w[i] + d*v[i]
//
//  Ejercita la maxima capacidad de la VPU:
//    4x vmv.v.x  (broadcast escalares)
//    4x vle32.v  (carga vectores)
//    4x vmul.vv  (multiplicacion elemento a elemento)
//    3x vadd.vv  (acumulacion)
//    1x vse32.v  (almacenamiento)
//  Total: 17 instrucciones PCPI por iteracion de 4 elementos
//  Ratio compute:memory = 7:4 (vs 2:3 del vadd simple)
//
//  Escalares: a=1000, b=751, c=503, d=257
//  Datos: x[i]=i+1, y[i]=2*(i+1), w[i]=3*(i+1), v[i]=4*(i+1)
//  Resultado esperado: z[i] = (1000+1502+1509+1028)*(i+1) = 5039*(i+1)
//
//  Mapa de memoria (N=64, 256 bytes cada arreglo):
//    VDATA_BASE + 0x000  vx[64]
//    VDATA_BASE + 0x100  vy[64]
//    VDATA_BASE + 0x200  vw[64]
//    VDATA_BASE + 0x300  vv[64]
//    VDATA_BASE + 0x400  z_s[64]  salida escalar
//    VDATA_BASE + 0x500  z_v[64]  salida vectorial
// =============================================================================

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS   10
#define N_ELEMS  64
#define ALPHA_A  1000
#define ALPHA_B  751
#define ALPHA_C  503
#define ALPHA_D  257

static volatile int32_t *const vx  = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vy  = (volatile int32_t*)(VDATA_BASE + 0x100);
static volatile int32_t *const vw  = (volatile int32_t*)(VDATA_BASE + 0x200);
static volatile int32_t *const vv  = (volatile int32_t*)(VDATA_BASE + 0x300);
static volatile int32_t *const z_s = (volatile int32_t*)(VDATA_BASE + 0x400);
static volatile int32_t *const z_v = (volatile int32_t*)(VDATA_BASE + 0x500);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: AXPY extendido\r\n");
    uart_puts("  z[i] = a*x[i] + b*y[i] + c*w[i] + d*v[i]\r\n");
    uart_puts("  a=1000 b=751 c=503 d=257, N=64, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    // Inicializar vectores de entrada
    for (int i = 0; i < N_ELEMS; i++) {
        vx[i] = i + 1;
        vy[i] = (i + 1) * 2;
        vw[i] = (i + 1) * 3;
        vv[i] = (i + 1) * 4;
    }

    // Verificacion de correctitud
    uart_puts("  Verificando correctitud...\r\n");
    sk_axpy_ext(vx, vy, vw, vv, z_s,
                ALPHA_A, ALPHA_B, ALPHA_C, ALPHA_D, N_ELEMS);
    vk_axpy_ext(vx, vy, vw, vv, z_v,
                ALPHA_A, ALPHA_B, ALPHA_C, ALPHA_D, N_ELEMS);

    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++) {
        int32_t expected = 5039 * (i + 1);
        if (z_s[i] != expected || z_v[i] != expected) {
            correct = 0;
            uart_puts("  FAIL en i="); uart_putdec((uint32_t)i); uart_nl();
            uart_puts("  z_s="); uart_putdec((uint32_t)z_s[i]);
            uart_puts("  z_v="); uart_putdec((uint32_t)z_v[i]);
            uart_puts("  exp="); uart_putdec((uint32_t)expected); uart_nl();
            break;
        }
    }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) { LEDS = LED_FAIL; while (1); }

    // Medicion escalar
    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS,
        sk_axpy_ext(vx, vy, vw, vv, z_s,
                    ALPHA_A, ALPHA_B, ALPHA_C, ALPHA_D, N_ELEMS));
    bench_print_stats("Escalar (AXPY-ext)", &s_esc);
    bench_print_runs(&s_esc);

    // Medicion vectorial
    uart_nl();
    uart_puts("  Midiendo vectorial...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS,
        vk_axpy_ext(vx, vy, vw, vv, z_v,
                    ALPHA_A, ALPHA_B, ALPHA_C, ALPHA_D, N_ELEMS));
    bench_print_stats("Vectorial (AXPY-ext)", &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison("AXPY-ext N=64", &s_esc, &s_vec, N_ELEMS);

    uart_nl();
    uart_separator();
    uart_puts("  RESUMEN\r\n");
    uart_separator();
    int hipot = bench_hypothesis_met(s_esc.mean, s_vec.mean);
    uart_puts("  Correcto:          "); uart_puts(correct          ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Hipotesis (>=30%): "); uart_puts(hipot ? "CUMPLIDA" : "NO CUMPLIDA"); uart_nl();
    uart_separator();

    LEDS = (correct && hipot) ? LED_OK : LED_FAIL;
    while (1);
    return 0;
}
