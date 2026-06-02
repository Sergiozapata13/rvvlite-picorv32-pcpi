// =============================================================================
//  main_axpy_sweep.c — Fase B: AXPY z=1000*x+y con N=32,64,128,256
//  Datos: x[i]=i+1, y[i]=(i+1)*10 → z[i]=1010*(i+1)
//  Memoria: 4 arreglos x 1024 bytes = 4KB desde VDATA_BASE
// =============================================================================
#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS 10
#define ALPHA  1000

static volatile int32_t *const vx  = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vy  = (volatile int32_t*)(VDATA_BASE + 0x400);
static volatile int32_t *const z_s = (volatile int32_t*)(VDATA_BASE + 0x800);
static volatile int32_t *const z_v = (volatile int32_t*)(VDATA_BASE + 0xC00);

static void run_n(int n) {
    for (int i = 0; i < n; i++) {
        vx[i] = i + 1;
        vy[i] = (i + 1) * 10;
    }
    sk_axpy(vx, vy, z_s, ALPHA, n);
    vk_axpy(vx, vy, z_v, ALPHA, n);
    int ok = 1;
    for (int i = 0; i < n; i++)
        if (z_s[i] != 1010*(i+1) || z_v[i] != 1010*(i+1)) { ok = 0; break; }

    bench_stats_t s_esc, s_vec;
    BENCH_RUN(s_esc, N_RUNS, sk_axpy(vx, vy, z_s, ALPHA, n));
    BENCH_RUN(s_vec, N_RUNS, vk_axpy(vx, vy, z_v, ALPHA, n));

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
    uart_puts("  FASE B: AXPY sweep\r\n");
    uart_puts("  z=1000*x+y, N in {32,64,128,256}\r\n");
    uart_separator();
    uart_nl();
    run_n(32);
    run_n(64);
    run_n(128);
    run_n(256);
    uart_nl();
    uart_separator();
    uart_puts("  FASE B AXPY COMPLETO\r\n");
    uart_separator();
    LEDS = LED_OK;
    while (1);
    return 0;
}
