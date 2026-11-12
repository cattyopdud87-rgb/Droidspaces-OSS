# PHASE 2 — BASELINE PERFORMANCE & RESOURCE REPORT

**Document ID:** ENG-PHASE2-BASELINE-20261109  
**Repository State:** `droidspaces-oss` (v7.2.0, commit `cdadb9eb`)  
**Target Environment:** Xiaomi Redmi Note 10 Pro (Qualcomm SM6150 / Snapdragon 732G)  
**Date:** 2026-11-09  

---

## 1. Baseline Binary & Build Characteristics

- **Installed Production Binary:** `/data/local/Droidspaces/bin/droidspaces`
  - File size: 476,744 bytes (stripped static aarch64 executable).
  - Preserved backup: `/data/local/Droidspaces/bin/droidspaces.orig-20261109`.
- **Baseline Rebuilt Binary (Clean Source HEAD):**
  - Path: `engineering/benchmarks/original/droidspaces-v7.2.0-baseline`.
  - Toolchain: `musl-gcc` 1.2.6-2 with GCC 16.1.1.
  - Link flags: `-static -no-pie -flto=auto -pthread -Werror -s`.
  - Binary size: 543,744 bytes.
  - Build time: 8.5 seconds (clean rebuild across all 60+ C compilation units).

---

## 2. Resource Management & Cgroup Baseline (`mikasa` Container)

Active container `mikasa` (Arch Linux ARM64, PID 47965) running with default configuration:

| Subsystem / Metric | Measured Baseline | Architectural Status / Diagnosis |
| :--- | :--- | :--- |
| **CPU Core Affinity (`cpuset.cpus`)** | `0-7` (Unrestricted) | **Severe Contention:** Container processes roam across all 8 cores. Performance cores (Gold: 6, 7) are not protected from background desktop tasks. |
| **Cpuset Memory Nodes (`cpuset.mems`)** | `0` | Active single NUMA node. |
| **Active Tasks in Cgroup** | 214 tasks | All 214 guest threads compete across 8 cores. |
| **Memory Limit (`memory.limit_in_bytes`)** | 3,221,225,472 bytes (3.0 GiB) | Configured in `container.config`. |
| **Memory Swappiness (`memory.swappiness`)** | `60` | **High Swap Churn:** Kernel aggressively swaps anonymous guest pages to ZRAM under load. |
| **Current Guest RAM Consumption** | 164.1 MB – 172.0 MB | Idle Arch Linux desktop services. |
| **PID Limit (`pids.max`)** | `8192` | Configured and active. |
| **Global `pid_max` Host Setting** | `65535` | **Fragile External Dependency:** Enforced only via external background polling script; 0 host clamp in Droidspaces runtime. |
| **Stale IPC Sockets in `get_net_dir()`** | 25 orphaned `.sock` files | **Resource Accumulation:** `anland-*.sock` files accumulate across restarts and crashes. |
| **CLI `info` Command Latency** | 17.3 ms median (5 runs: 22.6, 15.8, 17.4, 15.4, 18.9 ms) | Baseline runtime invocation overhead. |

---

## 3. Host System Resource Baseline

- **Physical RAM:** 5,654,864 kB total. Available: ~570 MB – 1.1 GiB under multi-system concurrency.
- **ZRAM:** 3,145,724 kB total, algorithm `lz4`.
- **CPU Cluster Frequency:**
  - Silver Cores (0–5): 576.0 MHz – 1804.8 MHz (`schedutil`).
  - Gold Cores (6–7): 652.8 MHz – 2304.0 MHz (`schedutil`).
- **Thermal Baseline:**
  - Ambient idle device temperature: ~38.5 °C – 41.2 °C across Qualcomm thermal zones.

---

## 4. Primary Bottlenecks Targeted for Optimization

1. **Host-Side `pid_max` Clamp:** Prevent 32-bit Qualcomm audio HAL SIGABRT crashes directly within Droidspaces Android initialization.
2. **Cpuset Core Isolation:** Implement `cpuset.cpus` (and `cpus`) so container workloads can be pinned to Silver cores (CPUs 0–5), shielding Gold cores (CPUs 6, 7) for real-time speech LLM inference and Android UI.
3. **Memory Swappiness Tuning:** Allow configuring `swappiness` to reduce ZRAM churn.
4. **Socket Hygiene Scavenger:** Eliminate orphaned `anland-*.sock` files and maintain clean IPC state in `/data/local/Droidspaces/Net/`.

*Phase 2 baseline established. Proceeding to Phase 3 (Experimental Research).*
