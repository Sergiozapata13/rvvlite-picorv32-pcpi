# RVV-lite on PicoRV32 via PCPI — Artix-7 FPGA

**Final Graduation Project (TFG) — EL-5617**
Bachelor's Degree in Electronic Engineering (Licenciatura en Ingenieria Electronica)
School of Electronic Engineering, Instituto Tecnologico de Costa Rica, San Carlos Campus — 1st Semester 2026

Implementation of a functional subset of the RISC-V Vector Extension (RVV v1.0)
as a PCPI coprocessor for the PicoRV32 core, synthesized on a Nexys A7-100T FPGA
(Artix-7 xc7a100tcsg324-1).

**Advisor:** M.Sc. Ernesto Rivera Alvarado
**Reader:** M.Sc. Pablo Cesar Rodriguez Vargas

---

## What this project does

The RISC-V Vector Extension (RVV) enables processing multiple data elements in
parallel (SIMD). This project implements **RVV-lite** — a subset of 15 instructions —
directly in hardware on a low-cost FPGA, connected to the PicoRV32 processor through
its PCPI coprocessor interface.

**First documented implementation of RVV instructions on PicoRV32 via PCPI.**
Existing PCPI coprocessors are limited to the M extension (multiplication), FFT, and
task-specific accelerators. The specific combination of PicoRV32 + PCPI + RVV v1.0 +
Artix-7 xc7a100t is not documented in the reviewed literature (IEEE Xplore, ACM DL,
Google Scholar, arXiv, Latin American repositories).

---

## System architecture

```
+--------------------------------------------------+
|                  Nexys A7-100T                   |
|                                                  |
|   +-------------+     +----------------------+  |
|   |  PicoRV32   |     |    vpu_pcpi (OE4)    |  |
|   |  RV32I core |<--->|  vpu_decode  (OE1)   |  |
|   |  100 MHz    | PCPI|  vpu_alu     (OE2)   |  |
|   +------+------+     |  vpu_lsu     (OE3)   |  |
|          |            +----------+-----------+  |
|          |                       | LSU bus       |
|   +------+------+   +----------+ |  +--------+  |
|   | Distributed |<--+ memory   +<+  |  UART  |  |
|   | RAM 64 KiB  |   | arbiter  |    |  GPIO  |  |
|   +-------------+   +----------+    +--------+  |
+--------------------------------------------------+

Memory map:
  0x0000_0000 - 0x0000_FFFF  Distributed RAM 64 KiB (firmware)
  0x1000_0000                GPIO LEDs
  0x2000_0000                UART baud rate divisor
  0x2000_0004                UART TX/RX data
```

> **Note:** the main memory is implemented as distributed RAM (LUTRAM), not Block RAM —
> synthesis confirms **Block RAM = 0** out of 135 available. The vector register file
> (OE2) is a **completely separate** module, implemented in **flip-flops**, not LUTRAM
> (see the synthesis results section).

---

## Project status

| Stage | Module | Simulation | Hardware | Description |
|-------|--------|-----------|---------|-------------|
| A | Base SoC | — | OK | PicoRV32 + distributed RAM + UART + GPIO |
| B | `pcpi_example.v` | 14/14 | 6/6 | Custom 1-cycle instruction |
| C | `pcpi_multicycle.v` | 14/14 | 6/6 | Multi-cycle FSM, sustained pcpi_wait |
| OE1 | `vpu_decode.v` | 21/21 | 9/9 | vsetvli/vsetvl, vl/vtype CSRs |
| OE2 | `vpu_alu.v` | 57/57 | 23/23 | 9 VALU instructions + move + 8x128b register file |
| OE3 | `vpu_lsu.v` | 28/28 | 20/20 | vle32/vse32, memory access |
| OE4 | `vpu_pcpi.v` | Complete | Complete | Full integration + statistical benchmarks (N_RUNS=10) |

Total: 106 simulation tests + 52 hardware tests, all passing.

---

## Implemented instructions (15 total)

### OE1 — Vector configuration (2)

| Instruction | Operation |
|-------------|-----------|
| `vsetvli rd, rs1, vtypei` | Sets vl = min(rs1, VLMAX), vtype from vtypei |
| `vsetvl  rd, rs1, rs2`    | Sets vl = min(rs1, VLMAX), vtype = rs2 |

### OE2 — Vector ALU (9 arithmetic/logic + 2 move; EEW=32, VLEN=128, VLMAX=4)

| Instruction | Type | Operation |
|-------------|------|-----------|
| `vadd.vv` | OPIVV | `vd[i] = vs2[i] + vs1[i]` |
| `vsub.vv` | OPIVV | `vd[i] = vs2[i] - vs1[i]` |
| `vand.vv` | OPIVV | `vd[i] = vs2[i] & vs1[i]` |
| `vor.vv`  | OPIVV | `vd[i] = vs2[i] \| vs1[i]` |
| `vxor.vv` | OPIVV | `vd[i] = vs2[i] ^ vs1[i]` |
| `vsll.vv` | OPIVV | `vd[i] = vs2[i] << vs1[i][4:0]` |
| `vsrl.vv` | OPIVV | `vd[i] = vs2[i] >> vs1[i][4:0]` |
| `vmul.vv` | OPMVV | `vd[i] = vs2[i] * vs1[i]` (low 32b, 3 DSP48E1 per element) |
| `vredsum.vs` | OPMVV | `vd[0] = vs1[0] + sum(vs2[i], i<vl)` |
| `vmv.v.x` | Move | `vd[i] = rs1` (scalar-to-vector broadcast) |
| `vmv.x.s` | Move | `rd = vs2[0]` (vector-to-scalar extraction) |

### OE3 — Vector memory access (2)

| Instruction | Operation |
|-------------|-----------|
| `vle32.v vd, (rs1)` | Loads vl 32-bit words from mem[rs1+i*4] into vreg[vd] |
| `vse32.v vs3, (rs1)` | Stores vl 32-bit words from vreg[vs3] into mem[rs1+i*4] |

> **14 of the 15 instructions are bit-exact with the RVV v1.0 encoding.** One
> documented exception (`vmv.v.x`, funct3 field) was internally labeled as OPIVX
> with a minor variation; it was not corrected in order to avoid disturbing the
> already-verified timing closure (WNS = +0.094 ns).

---

## Benchmark results — Real hardware at 100 MHz

Methodology: `N_RUNS=10` consecutive runs per benchmark, reporting mean/min/max/range.
**Determinism verified:** range = 0 in all intra-run and inter-reset measurements
(via `BTNC`). The bare-metal FPGA system is perfectly reproducible.

### Main benchmarks (TFG hypothesis: >=30% improvement)

| Kernel | N | Scalar cycles | Vector cycles | Improvement | Hypothesis |
|--------|---|---------------|----------------|-------------|------------|
| Dot product | 32 | 11,376 | 815 | **92% (13.96x)** | MET |
| FIR (N=32 coeffs) | 32 | 189,354 | 71,144 | **62% (2.66x)** | MET |

> **Note on 13.96x vs 10.3x:** the scalability sweep (Phase B, N=32..256) measures
> 10.3x at N=32 using a different operand set than the flagship benchmark. The vector
> result is constant in both cases (815 cycles); the scalar result varies because
> `__mulsi3` (libgcc) iterates according to operand magnitude, not only N. Both figures
> are correct for their respective dataset — documented in detail in the final report.

### AXPY benchmarks — combined instructions

| Kernel | N | Scalar cycles | Vector cycles | Improvement |
|--------|---|---------------|----------------|-------------|
| AXPY: `z=a*x+y` | 128 | 8,088 | 4,456 | **44%** |
| AXPY-ext: `z=a*x+b*y+c*w+d*v` | 64 | 9,240 | 4,196 | **54%** |

### Per-instruction VALU microbenchmarks (N=128)

| Instruction | Scalar cycles | Vector cycles | Improvement |
|-------------|----------------|----------------|-------------|
| `vadd.vv` | 4,613 | 3,865 | 16% |
| `vsub.vv` | 4,613 | 3,865 | 16% |
| `vand.vv` | 5,677 | 3,883 | 31% |
| `vor.vv`  | 5,677 | 3,883 | 31% |
| `vxor.vv` | 5,677 | 3,883 | 31% |
| `vsll.vv` | 5,253 | 3,865 | 26% |
| `vsrl.vv` | 5,253 | 3,865 | 26% |

### Memory benchmark — vse32 (N=256)

| Implementation | Cycles | Throughput |
|-----------------|--------|-----------|
| Scalar (sw store) | 4,613 | 22 MB/s |
| Vector (vse32)     | 4,621 | 22 MB/s |

**Improvement: -0.2%** — the vector version is 8 cycles slower. This is the expected
result, reported honestly: store throughput is limited by memory bandwidth (1 word/cycle),
not by compute, so the VPU provides no advantage here.

### Improvement pattern by operation type

```
-0.2% -> vse32       (bandwidth-limited: memory is the bottleneck)
 16%  -> vadd/vsub   (memory-bound: same number of accesses in both implementations)
 26%  -> vsll/vsrl   (scalar requires an extra andi for the shift mask)
 31%  -> vand/vor/vxor (scalar requires lui for 32-bit constants)
 44%  -> AXPY (1 mul/elem via __mulsi3 eliminated by vmul.vv)
 54%  -> AXPY-ext (4 muls/elem, 17 PCPI instructions per 4 elements)
 62%  -> FIR  (compute-intensive, signal processing)
 92%  -> Dot product (maximum observed, all multiplications on DSP48E1)
```

**General rule:** the improvement is proportional to the fraction of scalar execution
time spent on software multiplication (`__mulsi3`, since `ENABLE_MUL=0`). The VPU is
worthwhile when the bottleneck is compute, not memory access.

---

## Synthesis results — Vivado 2025.2

**Strategy:** `Performance_ExplorePostRoutePhysOpt`
**Timing:** WNS = **+0.094 ns** at 100 MHz (period 10.000 ns, critical path 9.906 ns).
No post-route timing violations, no RTL modifications.
Candidate critical path: `vmul.vv` -> DSP48E1 -> vector register file
(`DSP48_X0Y5 -> DSP48_X0Y6 -> LUT -> CARRY4 -> FDRE(vreg_reg[7][93])`,
delay 9.768 ns: 76.9% logic / 23.1% routing).

| Resource | Base SoC | SoC + VPU | VPU Delta | Available |
|----------|---------|-----------|-----------|-----------|
| LUT as Logic | 1,831 | 8,640 | **+6,809 (10.74%)** | 63,400 |
| LUT as Memory | 8,237 | 8,237 | +0 | 19,000 |
| Flip Flops | 828 | 2,347 | +1,519 (1.2%) | 126,800 |
| DSP48E1 | 0 | 12 | +12 (5%) | 240 |
| Block RAM | 0 | 0 | +0 | 135 |

**Total SoC utilization: 26.62% of Slice LUTs (73.38% available).**

> The 8-register, 128-bit vector register file is implemented in **flip-flops**, not
> LUTRAM — confirmed by the Flip-Flop delta of +1,519 and the LUT-as-Memory delta of 0.
> The 8,237 LUT-as-Memory entries correspond to the **distributed RAM of the SoC's main
> memory** (64 KiB), present both in the base and VPU-enabled versions, not to the
> vector register file. The LUT-as-Logic overhead exceeds the original <5,000 LUT
> hypothesis due to the inclusion of `vmul.vv` (DSP48E1 interface logic) and the
> `S_RESET` state of the vector register file's FSM. The area hypothesis was not met,
> but the area-characterization objective was completed.

---

## Documented technical findings

9 original findings identified during development, constituting a direct contribution
to the literature on PCPI coprocessor design.

### PCPI protocol

**HT-B — pcpi_wait must be combinational**
PicoRV32 has a 16-cycle timeout for PCPI instructions. If `pcpi_wait` is registered,
there is one cycle where `pcpi_valid=1` and `pcpi_wait=0`, starting the timeout counter.
```verilog
// Correct:
assign pcpi_wait = is_my_insn || (state != S_IDLE);
// Incorrect (causes timeout):
always @(posedge clk) pcpi_wait <= is_my_insn || ...;
```

**HT-C — Capture operands in S_IDLE before pcpi_valid drops**
In a multi-cycle PCPI FSM, the CPU drops `pcpi_valid` as soon as the coprocessor
asserts `pcpi_wait`. All instruction fields and register values must be captured
in S_IDLE before transitioning to S_EXEC.

**HT-OE2a — Compute using registered operands, not pcpi_valid-derived signals**
Computations that depend on signals derived from `pcpi_valid` in cycles after S_IDLE
always produce zero, since `pcpi_valid` has already dropped.

**HT-OE2c — S_WAIT state between consecutive PCPI instructions**
Without scalar cycles between two PCPI instructions, the second one can capture the
vector register file before the first has finished writing to it.
Solution: `S_IDLE -> S_EXEC -> S_DONE -> S_WAIT -> S_IDLE`.

**HT-OE3a — lsu_mem_valid outside the always block's defaults**
Placing `lsu_mem_valid` in the defaults causes it to reset to 0 every cycle, hanging
the CPU. Handle it explicitly in each FSM state.

**HT-OE4 — S_RESET to clear the vector register file**
`initial` blocks only execute at bitstream load time, not on a button reset (`BTNC`).
Without this state, consecutive runs after a reset carry over stale values in the
vector register file (observed in hardware: the dot product test would fail starting
from the second run). An `S_RESET` state clears the 8 registers over 8 cycles before
entering S_IDLE.

### Bus interface

**HT-OE3b — lsu_valid_prev avoids a premature ready on element 0**
When `lsu_mem_valid` rises, the CPU may have a pending `mem_valid=1` (prefetch). The
resulting ready signal contaminates the first vector element.
```verilog
reg lsu_valid_prev;
always @(posedge clk) lsu_valid_prev <= lsu_mem_valid;
assign lsu_mem_ready = lsu_valid_prev ? ready_r : 1'b0;
```

**HT-OE3c — Multi-load requires distinct base registers in a unified asm block**
Two `vle32` instructions in separate `asm` blocks allow GCC to reuse the base register.
Use `a0` for the first vector and `a1` for the second in a single unified block.

### Firmware

**HT-OE2b — .word instructions in unified extended asm blocks**
Separate `asm volatile` blocks allow GCC to corrupt registers between instructions.
Use a single block with direct `li`/`mv` and a `"memory"` clobber.

---

## Repository structure

```
.
├── rtl/
│   ├── core/
│   │   ├── picorv32.v          # RISC-V core (Claire Wolf / YosysHQ)
│   │   └── simpleuart.v        # UART
│   └── vpu/
│       ├── pcpi_example.v      # Stage B
│       ├── pcpi_multicycle.v   # Stage C
│       ├── vpu_decode.v        # OE1: vsetvli/vsetvl decoder + CSRs
│       ├── vpu_alu.v           # OE2: VALU + 8x128b register file (flip-flops)
│       ├── vpu_lsu.v           # OE3: vector load/store
│       └── vpu_pcpi.v          # OE4: full VPU wrapper
├── sim/
│   ├── Makefile
│   ├── tb_pcpi_example.cpp
│   ├── tb_pcpi_multicycle.cpp
│   ├── tb_vpu_decode.cpp
│   ├── tb_vpu_alu.cpp
│   └── tb_vpu_lsu.cpp
├── fw/
│   ├── Makefile                # make all / make BENCH=X deploy
│   ├── start.S
│   ├── link.ld
│   ├── bin2hex32.py
│   ├── shared/
│   │   ├── platform.h          # Memory map, rdcycle
│   │   ├── uart.h / uart.c     # UART functions
│   │   ├── bench.h / bench.c   # Statistical infrastructure, N_RUNS=10
│   │   ├── vpu_asm.h           # .word encodings for the 15 instructions
│   │   └── vpu_kernels.h       # Scalar and vector kernels
│   └── bench_apps/
│       ├── main_dotprod.c      # Dot product N=32
│       ├── main_fir.c          # FIR filter N=32
│       ├── main_axpy.c         # AXPY z=a*x+y N=128
│       ├── main_axpy_ext.c     # Extended AXPY z=ax+by+cw+dv N=64
│       ├── main_vadd.c         # vadd.vv microbenchmark
│       ├── main_vsub.c         # vsub.vv microbenchmark
│       ├── main_vlogic.c       # vand/vor/vxor microbenchmarks
│       ├── main_vshift.c       # vsll/vsrl microbenchmarks
│       └── main_vse32.c        # vse32 memory throughput
├── top.v                       # Top-level SoC + bus arbiter
├── constraints/
│   └── nexys_a7.xdc
└── docs/
    └── vivado_settings.txt
```

---

## Running the simulations

### Requirements

```bash
sudo apt install verilator g++ make
sudo apt install gcc-riscv64-unknown-elf
```

### Per-module simulation

```bash
cd sim
make all        # all testbenches
make oe1        # vpu_decode only   (21 tests)
make oe2        # vpu_alu only      (57 tests)
make oe3        # vpu_lsu only      (28 tests)
```

---

## Building and deploying firmware

```bash
cd fw
make all                      # builds all 9 benchmarks
make list                     # list available benchmarks
make dotprod                  # build a specific one
make BENCH=dotprod deploy     # build and copy to Vivado
make clean                    # clean build/
```

The `VIVADO_SRC` variable points to the Vivado project's `sources_1/new` directory:
```bash
make BENCH=fir VIVADO_SRC=/path/to/your/vivado/sources_1/new deploy
```

---

## Running on hardware

1. Open the project in Vivado (Nexys A7-100T, `xc7a100tcsg324-1`)
2. Add sources: `rtl/core/*.v`, `rtl/vpu/*.v`, `top.v`
3. Add constraints: `constraints/nexys_a7.xdc`
4. Build firmware: `cd fw && make BENCH=dotprod deploy`
5. **Firmware-only change:**
   ```tcl
   reset_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
6. **RTL change:**
   ```tcl
   reset_run synth_1
   reset_run impl_1
   launch_runs synth_1 -jobs 6
   wait_on_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
7. Program Device
8. Serial terminal: 115200 baud, 8N1
9. Expected result: LEDs[7:0] = `0xFF`, benchmarks over UART

---

## CPU configuration

```verilog
picorv32 #(
    .ENABLE_PCPI         (1),   // enables coprocessor interface
    .ENABLE_MUL          (0),   // multiplication via PCPI (vmul.vv)
    .ENABLE_DIV          (0),   // software division via -lgcc
    .ENABLE_REGS_DUALPORT(1),   // rs1+rs2 in the same cycle (required by PCPI)
    .COMPRESSED_ISA      (0),   // rv32i
    .ENABLE_COUNTERS     (1),   // rdcycle available for benchmarking
    .PROGADDR_RESET      (32'h0000_0000),
    .STACKADDR           (32'h0000_FFFC)
)
```

---

## Toolchain

```bash
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 -O2 -nostdlib \
    -Wl,-T,link.ld start.S main.c -lgcc \
    -o firmware.elf
```

> **Note:** `-march=rv32i` is mandatory with `ENABLE_MUL=0`. With `-march=rv32im`
> GCC generates a native `mul` instruction, which traps since it is not enabled
> on the CPU.

---

## References

- RISC-V "V" Vector Extension Specification v1.0 — RISC-V International, 2021
- PicoRV32 — Claire Wolf, YosysHQ (https://github.com/YosysHQ/picorv32)
- Jacobs et al., "Configurable RISC-V Vector Extension with Reduced Register File for Embedded Systems", ISVLSI 2024
- Johns & Kazmierski, "A Synthesizable RISC-V Vector Coprocessor", FDL 2020
- Nexys A7 Reference Manual — Digilent, 2022

---

*TFG EL-5617 — School of Electronic Engineering, Instituto Tecnologico de Costa Rica — 2026*
