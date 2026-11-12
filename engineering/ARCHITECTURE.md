# PHASE 1 — ARCHITECTURE & OPTIMIZATION AUDIT

**Document ID:** ENG-PHASE1-AUDIT-20261109  
**Repository:** `droidspaces-oss` (v7.2.0, commit `cdadb9eb`)  
**Target Device:** Qualcomm SM6150 / Snapdragon 732G (Redmi Note 10 Pro)  
**Date:** 2026-11-09  

---

## 1. Complete Architecture Overview

`droidspaces-oss` is a lightweight, high-performance container runtime designed for Linux and Android environments. It operates without dependencies on external runtimes (such as Docker, containerd, or runc), leveraging Linux kernel primitives directly:
- **Namespaces:** `CLONE_NEWPID`, `CLONE_NEWNS` (mount), `CLONE_NEWUTS`, `CLONE_NEWIPC`, `CLONE_NEWNET`, `CLONE_NEWCGROUP`, and optionally `CLONE_NEWUSER`.
- **Cgroups:** Dual-mode v1 and v2 controller hierarchy management.
- **Rootfs Engine:** `pivot_root`, bind mounts, and overlayfs (`volatile_mode`).
- **Network Engine:** Netlink sockets (`rtnetlink`), veth pairs, bridge, iptables/ip6tables masquerade and port-forwarding, plus DHCP and router advertisement daemons.
- **IPC & Android Display Bridges:** Wayland/Anland display daemon broker (`libdisplay_daemon`), Termux-X11 socket bridge, VirGL 3D renderer, and PulseAudio server.

---

## 2. Execution Flow & Critical Path Analysis

### 2.1 Container Startup Flow (`src/container/start.c`)
1. **CLI / Daemon Dispatch (`src/main.c`, `src/daemon/server.c`):**
   - Parses arguments and subcommands (`start`, `stop`, `run`, `enter`, `show`, `info`).
   - If daemon is running, commands proxy to `/data/local/Droidspaces/bin/droidspaces` via Unix domain socket; `DS_NO_PROXY=1` allows direct standalone execution.
2. **Pre-flight & Configuration (`start_rootfs_locked`):**
   - Resolves canonical rootfs paths.
   - Loads and parses configuration from `container.config` or `/data/local/Droidspaces/Containers/<name>/container.config`.
   - Invokes `android_optimizations(1)` asynchronously on Android hosts to adjust phantom process limits and doze modes.
3. **Host Networking Pre-configuration (`fix_networking_host`):**
   - Configures host-side veth interfaces, bridge, IP forwarding, and iptables rules.
4. **Monitor Process Fork (`ds_monitor_run`):**
   - Spawns monitor supervisor that oversees container lifetime, logs, and command socket.
5. **Container Init Fork & Namespace Creation (`internal_boot`):**
   - Clones with requested namespaces.
   - Sets up `/proc`, `/sys`, `/dev` (tmpfs), `/dev/pts`.
   - Executes `pivot_root` into container rootfs.
   - Applies cgroup limits (`ds_cgroup_apply_limits`).
   - Locks down `/proc/sys` via read-only bind mounts (`jail.c`).
   - Executes real init binary (e.g. systemd, OpenRC, busybox init).

### 2.2 Container Teardown Flow (`src/container/cleanup.c`)
1. Sends shutdown signal to container init (matching detected init family: `SIGRTMIN+3` for systemd, `SIGTERM`, etc.).
2. Shuts down Android auxiliary daemons (`ds_x11_daemon_stop`, `ds_anland_daemon_stop`, `ds_pulse_daemon_stop`).
3. If last container exits, calls `android_optimizations(0)` and restores SELinux enforcing.
4. Tears down network namespaces, veth interfaces, and iptables rules.
5. Unmounts overlays, images, and unlinks pid files.
6. Removes cgroup subtree in `/sys/fs/cgroup/droidspaces/<name>`.

---

## 3. Bottlenecks & Critical Vulnerabilities Identified

### Bottleneck 1: Absence of Host-Side `pid_max` Enforcement (P0 Reliability Defect)
- **Vulnerability:** Android Qualcomm proprietary audio HAL (`/vendor/lib/hw/audio.primary.sm6150.so`) is a 32-bit ELF (`ELF32`).
- **Mechanism:** In 32-bit Android Bionic libc, `pthread_mutex_t` stores the mutex owner TID in a 16-bit field. When any thread's TID exceeds 65,535, mutex lock operations abort with `SIGABRT` ("32-bit pthread_mutex_t only supports pids <= 65535"). This crashes `vendor.audio-hal`, `crash_dump32`, and triggers a cascading `audioserver` death-loop and framework soft-reboot.
- **Root Cause in Droidspaces:** When starting containers, host `pid_max` was previously raised or left unconstrained. Currently on this device, an external 2-second polling script (`99-audio-forensics.sh`) periodically clamps `pid_max` back to 65535, leaving a hazardous race window.
- **Architectural Solution:** In `src/android/android.c` (`android_optimizations(1)` and container initialization), Droidspaces must inspect `/proc/sys/kernel/pid_max` and enforce `pid_max <= 65535` directly before container init fork.

### Bottleneck 2: Lack of Cpuset Core Affinity Controller (P0 Performance Defect)
- **Problem:** The Snapdragon 732G features 2x Kryo 470 Gold performance cores (CPUs 6, 7 @ 2.304 GHz) and 6x Kryo 470 Silver efficiency cores (CPUs 0–5 @ 1.804 GHz).
- **Current Limitation in Droidspaces:** `src/cgroup/limits.c` only implements `memory`, `cpu` (CFS quota), and `pids`. It completely ignores `cpuset` (`cpuset.cpus`, `cpuset.mems`).
- **Consequence:** Container processes (KDE Plasma, Xwayland, PipeWire, Chromium) roam across all 8 cores. When an LLM (Qwen2.5 / llama.cpp) runs on Gold cores 6 and 7, container processes contend for CPU execution slices and evict L2/L3 cache lines, causing token generation speed to degrade by up to 50% (from 9.29 t/s down to 4.59 t/s) and introducing audio buffer underruns.
- **Architectural Solution:**
  1. Add `cpuset_cpus` (e.g. "0-5") and `cpuset_mems` (e.g. "0") configuration fields to `struct ds_config`.
  2. Implement `cpuset` controller handling in `src/cgroup/limits.c` for both cgroup v1 (`/sys/fs/cgroup/cpuset` / `/dev/cpuset`) and cgroup v2 (`cpuset.cpus`).
  3. Parse `cpuset=` in `container.config` and `--cpuset=` in CLI options (`src/cli/options.c`, `src/config/load.c`, `src/config/save.c`).

### Bottleneck 3: Accumulation of Orphaned IPC Sockets (`anland-*.sock`) (P1 Hygiene Defect)
- **Problem:** `src/android/anland/anland.c` generates a random UUID for the display broker socket on every start:
  `snprintf(cfg->anland_sock, sizeof(cfg->anland_sock), "%s/anland-%s.sock", get_net_dir(), uuid);`
- **Consequence:** 25 stale socket files (`anland-*.sock`) accumulated in `/data/local/Droidspaces/Net/` from past crashes, reboots, and unclean shutdowns.
- **Architectural Solution:**
  1. Make socket naming deterministic per container (`anland-<sanitized_name>.sock`) or add an automatic stale-socket scavenger that unlinks dead/unconnected sockets in `get_net_dir()` during daemon startup and container cleanup (`cleanup_container_resources`).
  2. Ensure socket unlinking is resilient even if `anland_sock` was not stored in config.

### Bottleneck 4: Swappiness and Memory Cgroup Tuning (P1 Resource Defect)
- **Problem:** Physical RAM is limited to 5.4 GiB with 3.0 GiB ZRAM. `memory_limit` in `limits.c` enforces `memory.limit_in_bytes`, but does not configure `memory.swappiness`. When container processes allocate memory, Linux global reclaim swaps pages aggressively to ZRAM, causing up to 7 MB/s swap churn.
- **Architectural Solution:** Add support for `swappiness` (0–100) in `container.config` and `--swappiness=` in CLI options, writing to `memory.swappiness` in the container's cgroup.

---

## 4. Prioritized Optimization Roadmap

| Priority | Component | Optimization Objective | Risk Profile |
| :--- | :--- | :--- | :--- |
| **P0** | `src/android/android.c` | Host-side `pid_max <= 65535` clamp in `android_optimizations` | Zero risk (essential for HAL stability) |
| **P0** | `src/cgroup/limits.c` | Implement `cpuset.cpus` & `cpuset.mems` for core isolation | Low risk (governed by config knob) |
| **P1** | `src/config/` & `src/cli/` | Expose `cpuset=` and `--cpuset=` across config & CLI | Low risk (additive changes) |
| **P1** | `src/container/cleanup.c` | Automated stale `anland-*.sock` sanitation in `get_net_dir()` | Low risk (unlinks only dead sockets) |
| **P2** | `src/cgroup/limits.c` | Expose `swappiness=` knob for memory cgroups | Low risk (defaults to system setting) |

---

## 5. Ecosystem Compatibility Matrix

- **`anland`:** Benefits from clean socket lifecycle management and deterministic socket naming, avoiding stale socket conflicts with Android consumer apps.
- **`Droidspaces-rootfs-Desktop-builder`:** Can include `cpuset=0-5` and `swappiness=10` in default desktop configuration templates (`Arch.Dockerfile`, `Debian-13.Dockerfile`), ensuring newly provisioned containers automatically run on efficiency cores.
- **`los_kernel_xiaomi_sm6150`:** Leverages existing kernel features (`CONFIG_CPUSETS=y`, `CONFIG_MEMCG=y`) without requiring out-of-tree kernel modifications.
- **Speech AI / LLM Applications:** Guarantees performance cores (CPUs 6, 7) remain dedicated to real-time speech synthesis and LLM inference without interference from background container processes.

*Phase 1 audit complete. Proceeding to Phase 2 (Baseline Establishment).*
