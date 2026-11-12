# PHASE 0 — ENVIRONMENT DISCOVERY REPORT

**Document ID:** ENG-PHASE0-DISCOVERY-20261109  
**Target Device:** Xiaomi Redmi Note 10 Pro (`sweet` / `sweetin`, M2101K6P)  
**Primary Target Repository:** `/data/data/com.termux/files/home/development/repo/droidspaces-oss/`  
**Date:** 2026-11-09  

---

## 1. Hardware & System Architecture

### 1.1 Compute Subsystem (SoC / CPU)
- **SoC:** Qualcomm SM6150 / Snapdragon 732G ("SDMMAGPIE")
- **CPU Architecture:** 8 Cores (ARMv8.2-A big.LITTLE / DynamIQ configuration):
  - **Gold Cluster (Performance):** 2× Kryo 470 Gold (Cortex-A76 derivative) @ 2.304 GHz (Cores 6, 7). Governor: `schedutil`, scaling frequencies: 652.8 MHz – 2304.0 MHz.
  - **Silver Cluster (Efficiency):** 6× Kryo 470 Silver (Cortex-A55 derivative) @ 1.804 GHz (Cores 0–5). Governor: `schedutil`, scaling frequencies: 576.0 MHz – 1804.8 MHz.
- **ISA Capabilities:**
  - `asimddp`: Native ARMv8.2 Dot Product (`SDOT`/`UDOT`) — hardware acceleration for quantized INT8 matrix operations (crucial for on-device LLM inference via llama.cpp).
  - `fphp`, `asimdhp`: Native half-precision floating-point arithmetic (FP16).
  - `atomics`: Large System Extensions (LSE atomics).
  - `crc32`, `aes`, `pmull`, `sha1`, `sha2`, `dcpop`.
- **GPU:** Qualcomm Adreno 618 with KGSL userspace driver and Freedreno Mesa support (`mesa-for-android-container`).
- **DSP / NPU:** Hexagon 692 DSP with FastRPC subsystem (`adsprpcd`, `adsprpcd_audiopd`).

### 1.2 Memory & Storage Subsystem
- **Physical RAM:** 5,654,864 kB (~5.4 GiB kernel visible, 6 GB physical LPDDR4X).
- **Available RAM:** ~570 MB – 1.5 GiB during multi-system concurrent operation.
- **ZRAM Swap:** 3,145,724 kB (3.0 GiB) on `/dev/block/zram0`.
  - Currently active algorithm: `lz4` (comp_algorithm: `lzo [lz4] deflate`).
  - Swapping statistics: High anonymous memory compression churn.
- **Storage:** UFS 2.2 storage backed by `f2fs` filesystem on `/data` (`dm-38`, dm-crypt encryption).
  - Free space on `/data`: ~28 GiB available.

### 1.3 Kernel & OS Runtime
- **Kernel Version:** Linux `4.14.357-reforge-gc52070689569` (ARM64 SMP PREEMPT).
- **OS Host:** LineageOS-family custom Android 15 ("reforge", SDK 35, display ID `BP1A.250405.007`).
- **Root Harness:** KernelSU (`ksud` binary, context `u:r:ksu:s0`, uid 0).
- **Cgroup Controllers Enabled:** `cpuset`, `cpu`, `cpuacct`, `schedtune`, `blkio`, `memory`, `freezer`, `pids`.
  - Note: Kernel 4.14 utilizes cgroups v1 hierarchy with partial v2 support. Droidspaces operates with `force_cgroupv1=1`.

---

## 2. Primary Target Repository Status

- **Path:** `/data/data/com.termux/files/home/development/repo/droidspaces-oss/`
- **Git State:** Clean working tree on branch `main` at commit `cdadb9eb` (tag: `v7.2.0`).
- **Architecture:**
  - C backend statically linked with musl libc (`src/`).
  - Android application in Kotlin/Jetpack Compose (`Android/`).
  - Modular subsystem layout: `cgroup/`, `container/`, `core/`, `daemon/`, `net/`, `rootfs/`, `security/`, `virt/`, `hw/`, `status/`, `cli/`.
- **Installed Runtime on Target Device:**
  - Binary: `/data/local/Droidspaces/bin/droidspaces` (476,744 bytes, ELF 64-bit LSB static executable).
  - Daemon socket: `/data/local/Droidspaces/droidspacesd.pid`.
  - Container active: `mikasa` (Arch Linux ARM64 rootfs, PID 47965, uptime > 21 hours).

---

## 3. Surrounding Development Ecosystem Survey

The neighboring repositories in `/data/data/com.termux/files/home/development/repo/` were inspected:

1. **`anland` (`legacy` branch @ `a23ccd8`):**
   - Inter-process communication display and audio bridge between container Wayland/PipeWire and Android host applications (`com.anland.consumer`, `com.anlandnext`).
   - Consumer app handles rendering via `/data/local/tmp/display_daemon.sock`.
   - Audio path currently relies on Unix domain sockets, accumulating orphaned sockets in `/data/local/Droidspaces/Net/` if not cleaned up.
2. **`Droidspaces-rootfs-Desktop-builder` (`main` branch @ `72a4c8e`):**
   - Automation recipes for Arch, Debian, Ubuntu, and Fedora container rootfs images with desktop environments.
   - Generates default container configurations (`container.config`).
3. **`droidspaces-package` (`main` branch @ `b417af4`):**
   - Houses custom distribution packages, notably the frozen `systemd-257` packages necessary for Arch Linux compatibility on kernel 4.14.
4. **`los_kernel_xiaomi_sm6150` (`stable` branch @ `9d63f2308`):**
   - Device tree and kernel source tree maintained for Redmi Note 10 Pro. Defconfig enables Droidspaces namespace and cgroup flags.
5. **`mesa-for-android-container` (`adreno-main` branch @ `4c94f3e82f9`):**
   - Adreno 618 KGSL Freedreno OpenGL/Vulkan container driver. Provides hardware graphics acceleration to container desktops.

---

## 4. Rollback and Safety Strategy

To guarantee zero regression risk and zero downtime:
1. **Installed Binary Backup:**
   `/data/local/Droidspaces/bin/droidspaces` was copied with preserved attributes to:
   `/data/local/Droidspaces/bin/droidspaces.orig-20261109`
2. **Git Working Tree Protection:**
   All changes in `droidspaces-oss` will be tracked on isolated git commits or branches. Uncommitted work can be restored instantly via `git checkout -- .`.
3. **Test Binary Isolation:**
   Per `AGENTS.md`, test builds are executed using `DS_NO_PROXY=1 /path/to/test-binary` without overwriting the running daemon until fully verified.
4. **Container State Protection:**
   Container `mikasa` configuration is backed up (`container.config.bak`).

---

## 5. Toolchains and Compilation Strategy

- **Native Termux:** Clang 21.1.8 targeting `aarch64-unknown-linux-android24` (Bionic).
- **Container Build Sandbox (`mikasa`):**
  - Architecture: Native aarch64 Arch Linux.
  - Compilers: GCC 16.1.1, Make 4.4.1.
  - Installed `musl` 1.2.6-2 with `musl-gcc` wrapper.
  - Enables compiling 100% statically linked musl binaries directly on-device with zero cross-compilation latency.

*Phase 0 discovery complete. Proceeding to Phase 1 (Architecture Audit).*
