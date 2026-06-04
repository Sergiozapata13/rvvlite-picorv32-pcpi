// =============================================================================
//  tb_pcpi_timing.cpp — Captura VCD del protocolo PCPI para diagrama temporal
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Secuencia capturada:
//    1. Ciclos idle (pcpi_wait=0, baseline)
//    2. vadd.vv  → muestra handshake completo (pcpi_rd=0, pcpi_wr=0, correcto)
//    3. Ciclos idle entre instrucciones
//    4. vmv.x.s  → muestra pcpi_rd=0x0000000B (v1[0]=11), pcpi_wr=1
//    5. Ciclos idle finales
//
//  Compilar:
//    verilator --cc --exe --build --trace \
//      -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-WIDTHTRUNC \
//      vpu_alu.v tb_pcpi_timing.cpp -o sim_pcpi_timing
//    ./obj_dir/sim_pcpi_timing
//
//  Ver en GTKWave:
//    gtkwave pcpi_timing.vcd
//    Signals: clk, pcpi_valid, pcpi_insn, pcpi_wait, pcpi_ready, pcpi_wr, pcpi_rd
// =============================================================================

#include "Vvpu_alu.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdio>

static Vvpu_alu*     dut;
static VerilatedVcdC* tfp;
static uint64_t      sim_time = 0;

// Tick completo con dump en flanco bajo y alto
static void tick() {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time * 10 + 0);   // nanosegundos: flanco bajo en t+0

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time * 10 + 5);   // flanco alto en t+5

    sim_time++;
}

// Eval sin tick (para señales combinacionales)
static void eval_only() {
    dut->eval();
    tfp->dump(sim_time * 10 + 2);   // punto intermedio sin clock
}

static void write_vreg_elem(int reg, int elem, uint32_t val) {
    dut->dbg_reg_sel  = reg;
    dut->dbg_elem_sel = elem;
    dut->dbg_we       = 1;
    dut->dbg_wdata    = val;
    tick();
    dut->dbg_we = 0;
    dut->eval();
}

// vadd.vv  v1 = v2 + v3   (funct6=000000, vm=1, funct3=000/OPIVV)
static uint32_t encode_vadd_v1_v2_v3() {
    return (0b000000 << 26) | (1 << 25) |
           (2 << 20) | (3 << 15) |
           (0b000 << 12) | (1 << 7) | 0b1010111;
}

// vmv.x.s  rd=x0 ← v1[0]  (funct6=010000, vm=1, vs2=1, funct3=010/OPMVV)
// pcpi_rd tendrá el valor de v1[0]
static uint32_t encode_vmvxs_v1() {
    return (0b010000 << 26) | (1 << 25) | (1 << 20) |
           (0b010 << 12) | (0 << 7) | 0b1010111;
}

// Ejecuta una instruccion PCPI y retorna los ciclos de latencia
static int run_insn(uint32_t insn, const char* name) {
    // Asegurar IDLE
    dut->pcpi_valid = 0;
    eval_only();
    int g = 8;
    while (dut->pcpi_wait && g-- > 0) tick();

    // Presentar instruccion en flanco de subida del clock
    dut->pcpi_valid = 1;
    dut->pcpi_insn  = insn;
    dut->eval();                    // eval combinacional: pcpi_wait debe subir
    tfp->dump(sim_time * 10 + 1);  // capturar estado combinacional en t+1

    int ok_wait = dut->pcpi_wait;
    printf("  Ciclo N   : pcpi_valid=1, pcpi_wait=%d %s\n",
           dut->pcpi_wait, ok_wait ? "[combinacional OK]" : "[ERROR: wait=0]");

    // Esperar pcpi_ready
    int cycles = 0;
    int timeout = 30;
    while (!dut->pcpi_ready && timeout-- > 0) {
        tick();
        cycles++;
    }

    if (dut->pcpi_ready) {
        printf("  Ciclo N+%d : pcpi_ready=1, pcpi_wr=%d, pcpi_rd=0x%08X\n",
               cycles, dut->pcpi_wr, dut->pcpi_rd);
        tick();   // capturar el pulso de ready completo
    } else {
        printf("  [TIMEOUT] %s no completó\n", name);
    }

    dut->pcpi_valid = 0;
    tick(); tick();  // ciclos de bajada + estabilizacion
    return cycles;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    dut = new Vvpu_alu;
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("pcpi_timing.vcd");

    printf("=== Captura VCD: protocolo PCPI — vadd.vv + vmv.x.s ===\n\n");

    // ── RESET ──────────────────────────────────────────────────────────
    dut->resetn       = 0;
    dut->pcpi_valid   = 0;
    dut->pcpi_insn    = 0;
    dut->pcpi_rs1     = 0;
    dut->pcpi_rs2     = 0;
    dut->csr_vl       = 4;
    dut->csr_vtype    = 0x010;
    dut->dbg_we       = 0;
    dut->dbg_wdata    = 0;
    dut->dbg_reg_sel  = 0;
    dut->dbg_elem_sel = 0;
    tick(); tick();
    dut->resetn = 1;
    // Esperar que S_RESET limpie todos los registros (8 ciclos FSM)
    for (int i = 0; i < 12; i++) tick();

    // ── CARGA DE DATOS: v2=[1,2,3,4], v3=[10,20,30,40] ────────────────
    printf("Cargando registros vectoriales...\n");
    for (int i = 0; i < 4; i++) write_vreg_elem(2, i, i + 1);
    for (int i = 0; i < 4; i++) write_vreg_elem(3, i, (i + 1) * 10);
    tick(); tick();

    // ── CICLOS IDLE VISIBLES (baseline para GTKWave) ───────────────────
    printf("Ciclos idle iniciales...\n");
    dut->pcpi_valid = 0;
    for (int i = 0; i < 5; i++) tick();

    // ── INSTRUCCION 1: vadd.vv v1 = v2 + v3 ───────────────────────────
    printf("\n[1] vadd.vv  v1 = v2 + v3\n");
    printf("    Esperado: v1 = [11, 22, 33, 44]\n");
    int lat1 = run_insn(encode_vadd_v1_v2_v3(), "vadd.vv");
    printf("    Latencia: %d ciclos\n", lat1);
    printf("    pcpi_rd=0 es correcto (vadd escribe en vreg, no en xreg)\n");

    // ── CICLOS IDLE ENTRE INSTRUCCIONES ───────────────────────────────
    for (int i = 0; i < 5; i++) tick();

    // ── INSTRUCCION 2: vmv.x.s  a0 = v1[0] ───────────────────────────
    printf("\n[2] vmv.x.s  x0 = v1[0]  (lee resultado de vadd)\n");
    printf("    Esperado: pcpi_rd = 0x0000000B (=11 decimal)\n");
    int lat2 = run_insn(encode_vmvxs_v1(), "vmv.x.s");
    printf("    Latencia: %d ciclos\n", lat2);

    // ── CICLOS IDLE FINALES ───────────────────────────────────────────
    for (int i = 0; i < 5; i++) tick();

    printf("\n=== Resumen ===\n");
    printf("Tiempo total simulado: %llu ns\n", (unsigned long long)sim_time);
    printf("VCD generado: pcpi_timing.vcd\n\n");
    printf("Señales a agregar en GTKWave (en orden):\n");
    printf("  clk, pcpi_valid, pcpi_insn[31:0], pcpi_wait,\n");
    printf("  pcpi_ready, pcpi_wr, pcpi_rd[31:0]\n\n");
    printf("Zoom recomendado:\n");
    printf("  vadd.vv:  buscar el primer pulso de pcpi_valid\n");
    printf("  vmv.x.s:  segundo pulso, donde pcpi_rd muestra 0x0000000B\n");

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}
