#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS   10
#define N_ELEMS  256
#define FILL_VAL 0x5A5A5A5A

static volatile int32_t *const out_s = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const out_v = (volatile int32_t*)(VDATA_BASE + 0x400);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: Throughput vse32\r\n");
    uart_puts("  N=256, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    sk_store_constant(out_s, (int32_t)FILL_VAL, N_ELEMS);
    vk_store_constant(out_v, (int32_t)FILL_VAL, N_ELEMS);

    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++)
        if (out_s[i] != out_v[i] || out_v[i] != (int32_t)FILL_VAL) {
            correct = 0; break;
        }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) { LEDS = LED_FAIL; while (1); }

    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS,
        sk_store_constant(out_s, (int32_t)FILL_VAL, N_ELEMS));
    bench_print_stats("Escalar (sw store)", &s_esc);
    bench_print_runs(&s_esc);

    uart_nl();
    uart_puts("  Midiendo vectorial (vse32)...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS,
        vk_store_constant(out_v, (int32_t)FILL_VAL, N_ELEMS));
    bench_print_stats("Vectorial (vse32)", &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison("vse32 N=256", &s_esc, &s_vec, N_ELEMS);

    // Throughput: bytes = N*4, tiempo = ciclos * 10ns (100MHz)
    // throughput_MBs = (N*4*100) / ciclos
    uint32_t bytes = N_ELEMS * 4;
    uint32_t tp_esc = (bytes * 100u) / s_esc.mean;
    uint32_t tp_vec = (bytes * 100u) / s_vec.mean;
    uart_nl();
    uart_puts("  --- Throughput ---\r\n");
    uart_print_dec("  Throughput escalar:   ", tp_esc);
    uart_puts("  MB/s\r\n");
    uart_print_dec("  Throughput vectorial: ", tp_vec);
    uart_puts("  MB/s\r\n");

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
