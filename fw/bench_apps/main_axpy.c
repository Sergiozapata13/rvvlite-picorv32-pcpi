// =============================================================================
//  main_axpy.c — Benchmark AXPY: z[i] = alpha * x[i] + y[i]
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  AXPY (Alpha X Plus Y) es el kernel BLAS Level-1 mas citado en literatura
//  de aceleracion vectorial. Combina vmul.vv + vadd.vv en un solo pass de
//  datos — ejercita la cadena multiply-accumulate element-wise completa.
//
//  Operacion: z[i] = alpha * x[i] + y[i],  i = 0..N-1
//  Datos: x[i] = (i+1), y[i] = (i+1)*10, alpha = 1000
//  Resultado esperado: z[i] = 1010*(i+1)
//
//  Instrucciones VPU ejercitadas: vsetvli, vmv.v.x, vle32.v (x2),
//                                  vmul.vv, vadd.vv, vse32.v
//
//  Mapa de memoria (N=128, 512 bytes cada arreglo):
//    VDATA_BASE + 0x000  x[128]
//    VDATA_BASE + 0x200  y[128]
//    VDATA_BASE + 0x400  z_s[128]  salida escalar
//    VDATA_BASE + 0x600  z_v[128]  salida vectorial
// =============================================================================

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS  10
#define N_ELEMS 128
#define ALPHA   1000

static volatile int32_t *const vx  = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vy  = (volatile int32_t*)(VDATA_BASE + 0x200);
static volatile int32_t *const z_s = (volatile int32_t*)(VDATA_BASE + 0x400);
static volatile int32_t *const z_v = (volatile int32_t*)(VDATA_BASE + 0x600);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: AXPY  z[i] = alpha*x[i] + y[i]\r\n");
    uart_puts("  alpha=1000, N=128, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    // Inicializar vectores
    for (int i = 0; i < N_ELEMS; i++) {
        vx[i] = i + 1;           // 1..128
        vy[i] = (i + 1) * 10;    // 10..1280
    }

    // Correctitud
    uart_puts("  Verificando correctitud...\r\n");
    sk_axpy(vx, vy, z_s, ALPHA, N_ELEMS);
    vk_axpy(vx, vy, z_v, ALPHA, N_ELEMS);

    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++) {
        int32_t expected = 1010 * (i + 1);
        if (z_s[i] != expected || z_v[i] != expected) { correct = 0; break; }
    }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) {
        // Debug primeros 4 elementos
        uart_puts("  z_s[0..3]: ");
        for (int i = 0; i < 4; i++) { uart_putdec((uint32_t)z_s[i]); uart_puts(" "); }
        uart_nl();
        uart_puts("  z_v[0..3]: ");
        for (int i = 0; i < 4; i++) { uart_putdec((uint32_t)z_v[i]); uart_puts(" "); }
        uart_nl();
        LEDS = LED_FAIL; while (1);
    }

    // Medicion escalar
    uart_nl();
    uart_puts("  Midiendo escalar (sk_axpy)...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS, sk_axpy(vx, vy, z_s, ALPHA, N_ELEMS));
    bench_print_stats("Escalar (AXPY)", &s_esc);
    bench_print_runs(&s_esc);

    // Medicion vectorial
    uart_nl();
    uart_puts("  Midiendo vectorial (vk_axpy)...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS, vk_axpy(vx, vy, z_v, ALPHA, N_ELEMS));
    bench_print_stats("Vectorial (AXPY)", &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison("AXPY N=128", &s_esc, &s_vec, N_ELEMS);

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
