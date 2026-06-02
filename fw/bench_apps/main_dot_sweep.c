// =============================================================================
//  main_dot_sweep.c — Fase B: dot product con N=32,64,128,256
//  Datos: a[i]=b[i]=(i+1)*10
//  Nota overflow: N=256 → max acum = sum((10i)^2,i=1..256)
//  Para evitar overflow usamos (i+1)*10 que da valores controlados
// =============================================================================
#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS 10

static volatile int32_t *const vec_a = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vec_b = (volatile int32_t*)(VDATA_BASE + 0x400);

static void run_n(int n) {
    for (int i = 0; i < n; i++) {
        vec_a[i] = (i + 1) * 10;
        vec_b[i] = (i + 1) * 10;
    }
    int32_t r_esc = sk_dot(vec_a, vec_b, n);
    int32_t r_vec = vk_dot(vec_a, vec_b, n);
    int ok = (r_esc == r_vec) ? 1 : 0;

    bench_stats_t s_esc, s_vec;
    BENCH_RUN(s_esc, N_RUNS, sk_dot(vec_a, vec_b, n));
    BENCH_RUN(s_vec, N_RUNS, vk_dot(vec_a, vec_b, n));

    uart_puts("N=");       uart_putdec(n);
    uart_puts(" ok=");     uart_puts(ok ? "1" : "0");
    uart_puts(" esc=");    uart_putdec(s_esc.mean);
    uart_puts(" vec=");    uart_putdec(s_vec.mean);
    uart_puts(" cpe_esc=");uart_putdec(s_esc.mean / n);
    uart_puts(" cpe_vec=");uart_putdec(s_vec.mean / n);
    uart_puts(" rng_e=");  uart_putdec(s_esc.range);
    uart_puts(" rng_v=");  uart_putdec(s_vec.range);
    uart_nl();
}

int main(void) {
    uart_init();
    LEDS = LED_BOOT;
    uart_nl();
    uart_separator();
    uart_puts("  FASE B: dot product sweep\r\n");
    uart_puts("  a[i]=b[i]=(i+1)*10, N in {32,64,128,256}\r\n");
    uart_separator();
    uart_nl();
    run_n(32);
    run_n(64);
    run_n(128);
    run_n(256);
    uart_nl();
    uart_separator();
    uart_puts("  FASE B dot COMPLETO\r\n");
    uart_separator();
    LEDS = LED_OK;
    while (1);
    return 0;
}
