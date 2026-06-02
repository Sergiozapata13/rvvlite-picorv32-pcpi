.libPaths(c("~/R/library", .libPaths()))
library(ggplot2)
library(tidyr)
library(dplyr)

# =============================================================
#  Fase B: Modelado descriptivo del sistema deterministico
#  TFG RVV-lite sobre PicoRV32 — Sergio Zapata, TEC 2026
#
#  Datos medidos en hardware Nexys A7-100T a 100 MHz
#  Todos los rangos = 0 (sistema deterministico verificado)
# =============================================================

# ── Dataset Fase B ────────────────────────────────────────────
datos <- data.frame(
  kernel = c(rep("vadd",4), rep("vadd",4),
             rep("AXPY",4), rep("AXPY",4),
             rep("dot",4),  rep("dot",4)),
  impl   = c(rep("Escalar",4),    rep("Vectorial",4),
             rep("Escalar",4),    rep("Vectorial",4),
             rep("Escalar",4),    rep("Vectorial",4)),
  N      = rep(c(32, 64, 128, 256), 6),
  ciclos = c(
    # vadd escalar
    1285, 2565, 5125, 10245,
    # vadd vectorial
    985,  1945, 3865, 7705,
    # AXPY escalar
    2190, 4334, 8622, 17198,
    # AXPY vectorial
    1148, 2252, 4460, 8876,
    # dot escalar
    8411, 18424, 40272, 87615,
    # dot vectorial
    815,  1591,  3143,  6247
  )
)
datos$cpe     <- datos$ciclos / datos$N
datos$kernel  <- factor(datos$kernel, levels=c("vadd","AXPY","dot"))
datos$impl    <- factor(datos$impl,   levels=c("Escalar","Vectorial"))

cat("=== Dataset Fase B ===\n")
print(datos)

# ── MODELO LINEAL: ciclos = beta*N + alpha ────────────────────
cat("\n\n=== MODELOS LINEALES: ciclos ~ N por kernel e implementacion ===\n")
cat(sprintf("%-12s %-12s %8s %8s %8s %8s\n",
    "Kernel","Impl","beta(c/e)","alpha","R2","RMSE"))
cat(strrep("-",60),"\n")

modelos <- list()
for (k in levels(datos$kernel)) {
  for (im in levels(datos$impl)) {
    sub <- datos[datos$kernel==k & datos$impl==im, ]
    m   <- lm(ciclos ~ N, data=sub)
    r2  <- summary(m)$r.squared
    b   <- coef(m)[2]
    a   <- coef(m)[1]
    rmse <- sqrt(mean(residuals(m)^2))
    cat(sprintf("%-12s %-12s %8.2f %8.1f %8.4f %8.2f\n",
        k, im, b, a, r2, rmse))
    modelos[[paste(k,im)]] <- m
  }
}

# ── SPEEDUP vs N ──────────────────────────────────────────────
cat("\n\n=== SPEEDUP vs N por kernel ===\n")
cat(sprintf("%-8s %8s %8s %8s %8s\n","Kernel","N=32","N=64","N=128","N=256"))
cat(strrep("-",44),"\n")
for (k in levels(datos$kernel)) {
  esc <- datos[datos$kernel==k & datos$impl=="Escalar",   "ciclos"]
  vec <- datos[datos$kernel==k & datos$impl=="Vectorial", "ciclos"]
  su  <- round(esc/vec, 2)
  cat(sprintf("%-8s %8.2f %8.2f %8.2f %8.2f\n", k, su[1],su[2],su[3],su[4]))
}

# ── DELTA DE CICLOS ───────────────────────────────────────────
cat("\n\n=== DELTA DE CICLOS (escalar - vectorial) ===\n")
cat(sprintf("%-8s %8s %8s %8s %8s\n","Kernel","N=32","N=64","N=128","N=256"))
cat(strrep("-",44),"\n")
for (k in levels(datos$kernel)) {
  esc <- datos[datos$kernel==k & datos$impl=="Escalar",   "ciclos"]
  vec <- datos[datos$kernel==k & datos$impl=="Vectorial", "ciclos"]
  d   <- esc - vec
  cat(sprintf("%-8s %8d %8d %8d %8d\n", k, d[1],d[2],d[3],d[4]))
}

# ── INTERACCION impl:N (hallazgo clave en dot) ────────────────
cat("\n\n=== INTERACCION impl:N — cpe por kernel y N ===\n")
cat(sprintf("%-10s %-12s %6s %6s %6s %6s\n","Kernel","Impl","N=32","N=64","N=128","N=256"))
cat(strrep("-",52),"\n")
for (k in levels(datos$kernel)) {
  for (im in levels(datos$impl)) {
    sub <- datos[datos$kernel==k & datos$impl==im, ]
    cat(sprintf("%-10s %-12s %6.1f %6.1f %6.1f %6.1f\n",
        k, im,
        sub$cpe[sub$N==32],  sub$cpe[sub$N==64],
        sub$cpe[sub$N==128], sub$cpe[sub$N==256]))
  }
}
cat("\nNota: dot escalar muestra cpe creciente con N\n")
cat("(valores mayores incrementan costo de __mulsi3)\n")
cat("dot vectorial mantiene cpe constante (DSP48E1 tiempo fijo)\n")

# ── GRAFICOS ──────────────────────────────────────────────────
# Fig 6: ciclos vs N con linea del modelo lineal
g6 <- ggplot(datos, aes(x=N, y=ciclos, color=impl, shape=impl)) +
  geom_point(size=4, alpha=0.9) +
  geom_smooth(method="lm", se=FALSE, linewidth=0.8, linetype="dashed") +
  facet_wrap(~kernel, scales="free_y") +
  scale_color_manual(values=c("Escalar"="#E74C3C","Vectorial"="#27AE60")) +
  scale_x_continuous(breaks=c(32,64,128,256)) +
  labs(title="Ciclos totales vs N — modelo lineal",
       subtitle="Puntos = medicion hardware. Linea = ajuste ciclos = beta*N + alpha",
       x="N (elementos)", y="Ciclos", color="Implementacion", shape="Implementacion") +
  theme_minimal(base_size=11) +
  theme(legend.position="bottom",
        strip.text=element_text(face="bold"))
ggsave("fig6_ciclos_vs_N.png", g6, width=11, height=5, dpi=150)

# Fig 7: speedup vs N
speedup_df <- data.frame(
  kernel  = rep(levels(datos$kernel), each=4),
  N       = rep(c(32,64,128,256), 3),
  speedup = c(
    1285/985,  2565/1945, 5125/3865,  10245/7705,
    2190/1148, 4334/2252, 8622/4460,  17198/8876,
    8411/815,  18424/1591,40272/3143, 87615/6247
  )
)
speedup_df$kernel <- factor(speedup_df$kernel, levels=c("vadd","AXPY","dot"))

g7 <- ggplot(speedup_df, aes(x=N, y=speedup, color=kernel, group=kernel)) +
  geom_line(linewidth=1.2) +
  geom_point(size=4) +
  geom_hline(yintercept=1, linetype="dashed", color="gray50") +
  scale_color_manual(values=c("vadd"="#3498DB","AXPY"="#F39C12","dot"="#E74C3C")) +
  scale_x_continuous(breaks=c(32,64,128,256)) +
  labs(title="Speedup vectorial vs N por tipo de kernel",
       subtitle="dot: speedup crece con N (interaccion impl:N). vadd/AXPY: speedup constante.",
       x="N (elementos)", y="Speedup = ciclos_esc / ciclos_vec",
       color="Kernel") +
  theme_minimal(base_size=11) +
  theme(legend.position="bottom")
ggsave("fig7_speedup_vs_N.png", g7, width=8, height=6, dpi=150)

# Fig 8: cpe vs N (muestra la diferencia entre kernels)
g8 <- ggplot(datos, aes(x=N, y=cpe, color=impl, shape=impl)) +
  geom_line(linewidth=1) +
  geom_point(size=4) +
  facet_wrap(~kernel, scales="free_y") +
  scale_color_manual(values=c("Escalar"="#E74C3C","Vectorial"="#27AE60")) +
  scale_x_continuous(breaks=c(32,64,128,256)) +
  labs(title="Ciclos por elemento (cpe) vs N",
       subtitle="vadd/AXPY: cpe constante (modelo lineal perfecto). dot escalar: cpe crece con N.",
       x="N (elementos)", y="Ciclos / Elemento",
       color="Implementacion", shape="Implementacion") +
  theme_minimal(base_size=11) +
  theme(legend.position="bottom",
        strip.text=element_text(face="bold"))
ggsave("fig8_cpe_vs_N.png", g8, width=11, height=5, dpi=150)

cat("\n\nGraficos: fig6_ciclos_vs_N.png, fig7_speedup_vs_N.png, fig8_cpe_vs_N.png\n")
cat("=== Fase B modelo completo ===\n")
