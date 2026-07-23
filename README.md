# RVV-lite sobre PicoRV32 via PCPI — FPGA Artix-7

**Trabajo Final de Graduacion (TFG) — EL-5617**
Licenciatura en Ingenieria Electronica — Escuela de Ingenieria Electronica
Instituto Tecnologico de Costa Rica, Sede San Carlos — I Semestre 2026

Implementacion de un subconjunto funcional de la extension vectorial RISC-V (RVV v1.0)
como coprocesador PCPI del nucleo PicoRV32, sintetizado en la FPGA Nexys A7-100T (Artix-7 xc7a100tcsg324-1).

**Tutor:** M.Sc. Ernesto Rivera Alvarado
**Lector:** M.Sc. Pablo Cesar Rodriguez Vargas

---

## Que hace este proyecto

La extension vectorial RISC-V (RVV) permite procesar multiples datos en paralelo (SIMD).
Este proyecto implementa **RVV-lite** — un subconjunto de 15 instrucciones — directamente
en hardware sobre una FPGA de bajo costo, conectado al procesador PicoRV32 mediante su
interfaz de coprocesador PCPI.

**Primera implementacion documentada de instrucciones RVV sobre PicoRV32 via PCPI.**
Los coprocesadores PCPI existentes se limitan a extension M (multiplicacion), FFT y
aceleradores de tarea especifica. La combinacion especifica PicoRV32 + PCPI + RVV v1.0 +
Artix-7 xc7a100t no esta documentada en la literatura revisada (IEEE Xplore, ACM DL,
Google Scholar, arXiv, repositorios latinoamericanos).

---

## Arquitectura del sistema

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
|          |                       | bus LSU       |
|   +------+------+   +----------+ |  +--------+  |
|   | RAM distr.  |<--+ arbitro  +<+  |  UART  |  |
|   | 64 KiB      |   | bus mem  |    |  GPIO  |  |
|   +-------------+   +----------+    +--------+  |
+--------------------------------------------------+

Mapa de memoria:
  0x0000_0000 - 0x0000_FFFF  RAM distribuida 64 KiB (firmware)
  0x1000_0000                GPIO LEDs
  0x2000_0000                UART divisor baudrate
  0x2000_0004                UART TX/RX dato
```

> **Nota:** la memoria principal se implementa como RAM distribuida (LUTRAM), no como
> Block RAM — la sintesis confirma **Block RAM = 0** de 135 disponibles. El banco de
> registros vectoriales (OE2) es un modulo **completamente separado**, implementado en
> **flip-flops**, no en LUTRAM (ver seccion de sintesis).

---

## Estado del proyecto

| Etapa | Modulo | Simulacion | Hardware | Descripcion |
|-------|--------|-----------|---------|-------------|
| A | SoC base | — | OK | PicoRV32 + RAM distribuida + UART + GPIO |
| B | `pcpi_example.v` | 14/14 | 6/6 | Instruccion custom 1 ciclo |
| C | `pcpi_multicycle.v` | 14/14 | 6/6 | FSM multiciclo, pcpi_wait sostenido |
| OE1 | `vpu_decode.v` | 21/21 | 9/9 | vsetvli/vsetvl, CSRs vl/vtype |
| OE2 | `vpu_alu.v` | 57/57 | 23/23 | VALU 9 instrucciones + movimiento + banco 8x128b |
| OE3 | `vpu_lsu.v` | 28/28 | 20/20 | vle32/vse32, acceso a memoria |
| OE4 | `vpu_pcpi.v` | Completo | Completo | Integracion + benchmarks estadisticos (N_RUNS=10) |

Total: 106 pruebas de simulacion + 52 pruebas de hardware, todas superadas.

---

## Instrucciones implementadas (15 total)

### OE1 — Configuracion vectorial (2)

| Instruccion | Operacion |
|-------------|-----------|
| `vsetvli rd, rs1, vtypei` | Configura vl = min(rs1, VLMAX), vtype segun vtypei |
| `vsetvl  rd, rs1, rs2`    | Configura vl = min(rs1, VLMAX), vtype = rs2 |

### OE2 — VALU vectorial (9 aritmetico-logicas + 2 movimiento; EEW=32, VLEN=128, VLMAX=4)

| Instruccion | Tipo | Operacion |
|-------------|------|-----------|
| `vadd.vv` | OPIVV | `vd[i] = vs2[i] + vs1[i]` |
| `vsub.vv` | OPIVV | `vd[i] = vs2[i] - vs1[i]` |
| `vand.vv` | OPIVV | `vd[i] = vs2[i] & vs1[i]` |
| `vor.vv`  | OPIVV | `vd[i] = vs2[i] \| vs1[i]` |
| `vxor.vv` | OPIVV | `vd[i] = vs2[i] ^ vs1[i]` |
| `vsll.vv` | OPIVV | `vd[i] = vs2[i] << vs1[i][4:0]` |
| `vsrl.vv` | OPIVV | `vd[i] = vs2[i] >> vs1[i][4:0]` |
| `vmul.vv` | OPMVV | `vd[i] = vs2[i] * vs1[i]` (32b bajo, 3 DSP48E1 por elemento) |
| `vredsum.vs` | OPMVV | `vd[0] = vs1[0] + sum(vs2[i], i<vl)` |
| `vmv.v.x` | Movimiento | `vd[i] = rs1` (broadcast escalar-a-vector) |
| `vmv.x.s` | Movimiento | `rd = vs2[0]` (extraccion vector-a-escalar) |

### OE3 — Acceso a memoria vectorial (2)

| Instruccion | Operacion |
|-------------|-----------|
| `vle32.v vd, (rs1)` | Carga vl palabras de 32b desde mem[rs1+i*4] a vreg[vd] |
| `vse32.v vs3, (rs1)` | Escribe vl palabras de 32b desde vreg[vs3] a mem[rs1+i*4] |

> **14 de las 15 instrucciones son bit-exactas con la codificacion RVV v1.0.** Una
> excepcion documentada (`vmv.v.x`, campo funct3) se etiqueto internamente como OPIVX
> con una variacion menor; no se corrigio para no perturbar el cierre de timing ya
> verificado (WNS = +0.094 ns).

---

## Resultados de benchmarks — Hardware real a 100 MHz

Metodologia: `N_RUNS=10` corridas consecutivas por benchmark, se reportan media/min/max/rango.
**Determinismo verificado:** rango = 0 en todas las mediciones intra-corrida
e inter-reset (via `BTNC`). El sistema bare-metal sobre FPGA es perfectamente reproducible.

### Benchmarks principales (hipotesis del TFG: >=30% mejora)

| Kernel | N | Ciclos esc. | Ciclos vec. | Mejora | Hipotesis |
|--------|---|------------|------------|--------|-----------|
| Dot product | 32 | 11,376 | 815 | **92% (13.96x)** | CUMPLIDA |
| FIR (N=32 coefs) | 32 | 189,354 | 71,144 | **62% (2.66x)** | CUMPLIDA |

> **Nota sobre 13.96x vs 10.3x:** el barrido de escalabilidad (Fase B, N=32..256) mide
> 10.3x en N=32 con un conjunto de operandos distinto al del benchmark insignia. El
> vector es constante en ambos casos (815 ciclos); el escalar varia porque `__mulsi3`
> (libgcc) itera segun la magnitud de los operandos, no solo segun N. Ambos numeros son
> correctos para su respectivo conjunto de datos — documentado en detalle en el informe.

### Benchmarks AXPY — instrucciones combinadas

| Kernel | N | Ciclos esc. | Ciclos vec. | Mejora |
|--------|---|------------|------------|--------|
| AXPY: `z=a*x+y` | 128 | 8,088 | 4,456 | **44%** |
| AXPY-ext: `z=a*x+b*y+c*w+d*v` | 64 | 9,240 | 4,196 | **54%** |

### Microbenchmarks por instruccion VALU (N=128)

| Instruccion | Ciclos esc. | Ciclos vec. | Mejora |
|-------------|------------|------------|--------|
| `vadd.vv` | 4,613 | 3,865 | 16% |
| `vsub.vv` | 4,613 | 3,865 | 16% |
| `vand.vv` | 5,677 | 3,883 | 31% |
| `vor.vv`  | 5,677 | 3,883 | 31% |
| `vxor.vv` | 5,677 | 3,883 | 31% |
| `vsll.vv` | 5,253 | 3,865 | 26% |
| `vsrl.vv` | 5,253 | 3,865 | 26% |

### Benchmark de memoria — vse32 (N=256)

| Implementacion | Ciclos | Throughput |
|----------------|--------|-----------|
| Escalar (sw store) | 4,613 | 22 MB/s |
| Vectorial (vse32)  | 4,621 | 22 MB/s |

**Mejora: -0.2%** — la version vectorial es 8 ciclos mas lenta. Resultado esperado y
reportado honestamente: el throughput de store esta limitado por el ancho de banda de
la memoria (1 palabra/ciclo), no por computo, asi que la VPU no aporta ventaja aqui.

### Patron de mejora por tipo de operacion

```
-0.2% -> vse32       (bandwidth-limited: la memoria es el cuello de botella)
 16%  -> vadd/vsub   (memory-bound: mismo numero de accesos en ambas impl.)
 26%  -> vsll/vsrl   (escalar requiere andi adicional para mascara de shift)
 31%  -> vand/vor/vxor (escalar requiere lui para constantes de 32 bits)
 44%  -> AXPY (1 mul/elem via __mulsi3 eliminada por vmul.vv)
 54%  -> AXPY-ext (4 muls/elem, 17 instrucciones PCPI por 4 elementos)
 62%  -> FIR  (compute-intensive, procesamiento de senales)
 92%  -> Dot product (maximo observado, todas las multiplicaciones en DSP48E1)
```

**Regla general:** la mejora es proporcional a la fraccion de tiempo escalar consumida
en multiplicacion por software (`__mulsi3`, ya que `ENABLE_MUL=0`). La VPU conviene
cuando el cuello de botella es el computo, no el acceso a memoria.

---

## Resultados de sintesis — Vivado 2025.2

**Estrategia:** `Performance_ExplorePostRoutePhysOpt`
**Timing:** WNS = **+0.094 ns** a 100 MHz (periodo 10.000 ns, camino critico 9.906 ns).
Sin violaciones de timing post-ruteo, sin modificaciones al RTL.
Camino critico candidato: `vmul.vv` -> DSP48E1 -> banco de registros vectoriales
(`DSP48_X0Y5 -> DSP48_X0Y6 -> LUT -> CARRY4 -> FDRE(vreg_reg[7][93])`,
delay 9.768 ns: 76.9% logica / 23.1% ruteo).

| Recurso | SoC base | SoC + VPU | Delta VPU | Disponible |
|---------|---------|-----------|-----------|-----------|
| LUT as Logic | 1,831 | 8,640 | **+6,809 (10.74%)** | 63,400 |
| LUT as Memory | 8,237 | 8,237 | +0 | 19,000 |
| Flip Flops | 828 | 2,347 | +1,519 (1.2%) | 126,800 |
| DSP48E1 | 0 | 12 | +12 (5%) | 240 |
| Block RAM | 0 | 0 | +0 | 135 |

**Utilizacion total del SoC completo: 26.62% de los Slice LUTs (73.38% disponible).**

> El banco de 8 registros vectoriales de 128 bits se implementa en **flip-flops**, no en
> LUTRAM — confirmado por delta de Flip-Flops = +1,519 y delta de LUT as Memory = 0. Los
> 8,237 LUT as Memory corresponden a la **RAM distribuida de la memoria principal del SoC**
> (64 KiB), presente tanto en la version base como en la version con VPU, no al banco
> vectorial. El overhead de LUT as Logic supera la hipotesis original de <5,000 LUT por
> la inclusion de `vmul.vv` (interfaz a DSP48E1) y el estado `S_RESET` de la FSM del
> banco vectorial. La hipotesis de area no se cumplio, pero el objetivo de
> caracterizacion de area si se completo.

---

## Hallazgos tecnicos documentados

9 hallazgos originales identificados durante el desarrollo, que constituyen
aporte directo a la literatura de diseno de coprocesadores PCPI.

### Protocolo PCPI

**HT-B — pcpi_wait debe ser combinacional**
El PicoRV32 tiene timeout de 16 ciclos. Si `pcpi_wait` se registra, el contador
comienza antes de que el coprocesador lo aserte.
```verilog
// Correcto:
assign pcpi_wait = is_my_insn || (state != S_IDLE);
// Incorrecto (genera timeout):
always @(posedge clk) pcpi_wait <= is_my_insn || ...;
```

**HT-C — Capturar operandos en S_IDLE antes de que pcpi_valid baje**
El CPU baja `pcpi_valid` en cuanto el coprocesador aserta `pcpi_wait`. Capturar
todos los operandos en S_IDLE antes de transicionar a S_EXEC.

**HT-OE2a — Calcular con operandos registrados, no con senales de pcpi_valid**
Calculos que dependen de senales derivadas de `pcpi_valid` en ciclos posteriores
a S_IDLE producen cero porque `pcpi_valid` ya bajo.

**HT-OE2c — Estado S_WAIT entre instrucciones PCPI consecutivas**
Sin ciclos escalares entre dos instrucciones PCPI, la segunda puede capturar
el banco vectorial antes de que la primera complete su escritura.
Solucion: `S_IDLE -> S_EXEC -> S_DONE -> S_WAIT -> S_IDLE`.

**HT-OE3a — lsu_mem_valid fuera de defaults del always block**
`lsu_mem_valid` en los defaults causa reset a 0 cada ciclo, colgando al CPU.
Manejar explicitamente en cada estado de la FSM.

**HT-OE4 — S_RESET para limpiar el banco vectorial**
`initial` solo ejecuta al cargar el bitstream, no al presionar reset (`BTNC`).
Sin este estado, corridas sucesivas tras reset arrastran valores residuales del
banco vectorial (se observo en hardware: dot product fallaba desde la segunda
ejecucion). Estado `S_RESET` que limpia los 8 registros en 8 ciclos antes de S_IDLE.

### Interfaz de bus

**HT-OE3b — lsu_valid_prev evita ready prematuro en elemento 0**
Cuando `lsu_mem_valid` sube, el CPU puede tener `mem_valid=1` pendiente (prefetch).
El ready resultante contamina el primer elemento vectorial.
```verilog
reg lsu_valid_prev;
always @(posedge clk) lsu_valid_prev <= lsu_mem_valid;
assign lsu_mem_ready = lsu_valid_prev ? ready_r : 1'b0;
```

**HT-OE3c — Multi-load con registros base distintos en bloque asm unificado**
Dos `vle32` en bloques `asm` separados permiten que GCC reutilice el registro base.
Usar `a0` para el primer vector y `a1` para el segundo en un bloque unificado.

### Firmware

**HT-OE2b — Instrucciones .word en bloques asm extendidos unificados**
Bloques `asm volatile` separados permiten que GCC corrompa registros entre instrucciones.
Usar un solo bloque con `li`/`mv` directos y clobber `"memory"`.

---

## Estructura del repositorio

```
.
├── rtl/
│   ├── core/
│   │   ├── picorv32.v          # Nucleo RISC-V (Claire Wolf / YosysHQ)
│   │   └── simpleuart.v        # UART
│   └── vpu/
│       ├── pcpi_example.v      # Etapa B
│       ├── pcpi_multicycle.v   # Etapa C
│       ├── vpu_decode.v        # OE1: decodificador vsetvli/vsetvl + CSRs
│       ├── vpu_alu.v           # OE2: VALU + banco 8x128b (flip-flops)
│       ├── vpu_lsu.v           # OE3: Load/Store vectorial
│       └── vpu_pcpi.v          # OE4: wrapper VPU completa
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
│   │   ├── platform.h          # Mapa de memoria, rdcycle
│   │   ├── uart.h / uart.c     # Funciones UART
│   │   ├── bench.h / bench.c   # Infraestructura estadistica N_RUNS=10
│   │   ├── vpu_asm.h           # Encodings .word de las 15 instrucciones
│   │   └── vpu_kernels.h       # Kernels escalares y vectoriales
│   └── bench_apps/
│       ├── main_dotprod.c      # Dot product N=32
│       ├── main_fir.c          # Filtro FIR N=32
│       ├── main_axpy.c         # AXPY z=a*x+y N=128
│       ├── main_axpy_ext.c     # AXPY extendido z=ax+by+cw+dv N=64
│       ├── main_vadd.c         # Microbenchmark vadd.vv
│       ├── main_vsub.c         # Microbenchmark vsub.vv
│       ├── main_vlogic.c       # Microbenchmarks vand/vor/vxor
│       ├── main_vshift.c       # Microbenchmarks vsll/vsrl
│       └── main_vse32.c        # Throughput memoria vse32
├── top.v                       # Top-level SoC + arbitro de bus
├── constraints/
│   └── nexys_a7.xdc
└── docs/
    └── vivado_settings.txt
```

---

## Como correr las simulaciones

### Requisitos

```bash
sudo apt install verilator g++ make
sudo apt install gcc-riscv64-unknown-elf
```

### Simulacion por modulo

```bash
cd sim
make all        # todos los testbenches
make oe1        # solo vpu_decode   (21 tests)
make oe2        # solo vpu_alu      (57 tests)
make oe3        # solo vpu_lsu      (28 tests)
```

---

## Como compilar y desplegar firmware

```bash
cd fw
make all                      # compila los 9 benchmarks
make list                     # ver benchmarks disponibles
make dotprod                  # compilar uno especifico
make BENCH=dotprod deploy     # compilar y copiar a Vivado
make clean                    # limpiar build/
```

La variable `VIVADO_SRC` apunta al directorio `sources_1/new` del proyecto Vivado:
```bash
make BENCH=fir VIVADO_SRC=/ruta/a/tu/proyecto/sources_1/new deploy
```

---

## Como correr en hardware

1. Abrir proyecto en Vivado (Nexys A7-100T, `xc7a100tcsg324-1`)
2. Agregar sources: `rtl/core/*.v`, `rtl/vpu/*.v`, `top.v`
3. Agregar constraints: `constraints/nexys_a7.xdc`
4. Compilar firmware: `cd fw && make BENCH=dotprod deploy`
5. **Cambio de firmware solamente:**
   ```tcl
   reset_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
6. **Cambio de RTL:**
   ```tcl
   reset_run synth_1
   reset_run impl_1
   launch_runs synth_1 -jobs 6
   wait_on_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
7. Program Device
8. Terminal serie: 115200 baud, 8N1
9. Resultado esperado: LEDs[7:0] = `0xFF`, benchmarks por UART

---

## Configuracion del CPU

```verilog
picorv32 #(
    .ENABLE_PCPI         (1),   // interfaz de coprocesador
    .ENABLE_MUL          (0),   // multiplicacion via PCPI (vmul.vv)
    .ENABLE_DIV          (0),   // division software via -lgcc
    .ENABLE_REGS_DUALPORT(1),   // rs1+rs2 en mismo ciclo (requerido PCPI)
    .COMPRESSED_ISA      (0),   // rv32i
    .ENABLE_COUNTERS     (1),   // rdcycle para benchmarks
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

> **Nota:** `-march=rv32i` es obligatorio con `ENABLE_MUL=0`. Con `-march=rv32im`
> GCC genera instruccion `mul` que causa trap al no estar habilitada en el CPU.

---

## Referencias

- RISC-V "V" Vector Extension Specification v1.0 — RISC-V International, 2021
- PicoRV32 — Claire Wolf, YosysHQ (https://github.com/YosysHQ/picorv32)
- Jacobs et al., "Configurable RISC-V Vector Extension with Reduced Register File for Embedded Systems", ISVLSI 2024
- Johns & Kazmierski, "A Synthesizable RISC-V Vector Coprocessor", FDL 2020
- Nexys A7 Reference Manual — Digilent, 2022

---

*TFG EL-5617 — Escuela de Ingenieria Electronica, Instituto Tecnologico de Costa Rica — 2026*
