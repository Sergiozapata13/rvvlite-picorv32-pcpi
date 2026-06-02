// =============================================================================
//  main_vadd_sweep.c — Fase B: vadd.vv con N=32,64,128,256
//  Usa sk_add / vk_add_vv (nombres reales en vpu_kernels.h)
//  Datos: a[i]=i+1, b[i]=N-i → out[i]=N+1 (verificable)
//  Memoria: 4 arreglos x 1024 bytes = 4KB desde VDATA_BASE
// =============================================================================
#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS 10

static volatile int32_t *const va    = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vb    = (volatile int32_t*)(VDATA_BASE + 0x400);
static volatile int32_t *const out_s = (volatile int32_t*)(VDATA_BASE + 0x800);
static volatile int32_t *const out_v = (volatile int32_t*)(VDATA_BASE + 0xC00);

static void run_n(int n) {
    for (int i = 0; i < n; i++) {
        va[i] = i + 1;
        vb[i] = n - i;
    }
    sk_add(va, vb, out_s, n);
    vk_add_vv(va, vb, out_v, n);
    int ok = 1;
    for (int i = 0; i < n; i++)
        if (out_s[i] != (n + 1) || out_v[i] != (n + 1)) { ok = 0; break; }

    bench_stats_t s_esc, s_vec;
    BENCH_RUN(s_esc, N_RUNS, sk_add(va, vb, out_s, n));
    BENCH_RUN(s_vec, N_RUNS, vk_add_vv(va, vb, out_v, n));

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
    uart_puts("  FASE B: vadd.vv sweep\r\n");
    uart_puts("  out[i]=N+1, N in {32,64,128,256}\r\n");
    uart_separator();
    uart_nl();
    run_n(32);
    run_n(64);
    run_n(128);
    run_n(256);
    uart_nl();
    uart_separator();
    uart_puts("  FASE B vadd COMPLETO\r\n");
    uart_separator();
    LEDS = LED_OK;
    while (1);
    return 0;
}
