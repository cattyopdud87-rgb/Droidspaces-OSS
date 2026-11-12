# DROIDSPACES OPTIMIZATION LOG

**Document ID:** ENG-OPTIMIZATION-LOG-20261109  
**Repository:** `droidspaces-oss`  
**Date:** 2026-11-09  

---

### Optimization Entry 001: Host-Side `pid_max <= 65535` Enforcement
- **Component:** `src/android/android.c` (`android_optimizations`)
- **Original Behavior:** Droidspaces did not inspect or clamp `/proc/sys/kernel/pid_max` during startup. When `pid_max` exceeded 65535, 32-bit Qualcomm vendor HAL (`audio.primary.sm6150.so`) aborts with SIGABRT due to Bionic 16-bit TID limitation in `pthread_mutex_t`. An external 2-second polling script was required to mitigate the crash, leaving a dangerous race window.
- **New Behavior:** `android_optimizations(1)` checks `/proc/sys/kernel/pid_max` before container init fork. If `pid_max > 65535` or invalid, it clamps it directly to 65535 via atomic write.
- **Classification:** Reliability-focused / High-value.
- **Validation:** Live execution on hardware verified clamp and eliminated race window.

---

### Optimization Entry 002: Cgroup `cpuset.cpus` Core Affinity Controller
- **Component:** `src/cgroup/limits.c`, `src/config/`, `src/cli/`, `src/virt/affinity.c`
- **Original Behavior:** Droidspaces only implemented CFS quota (`cpu.cfs_quota_us`) and memory limits. It completely lacked `cpuset` controller integration. Container tasks roamed freely across all 8 cores (Gold and Silver clusters), causing CPU cache evictions and contending with Android UI rendering and real-time LLM inference.
- **New Behavior:**
  - Added `cpuset_cpus` and `cpuset_mems` configuration parameters to `struct ds_config`.
  - Added support for `cpuset=` and `cpuset_cpus=` in `container.config` and `--cpuset=` in CLI options.
  - Implemented `cpuset.cpus` and `cpuset.mems` enforcement in `ds_cgroup_apply_limits()`, automatically handling Android's noprefix cpuset files (`cpus`, `mems`) as well as standard Linux/v2 files (`cpuset.cpus`, `cpuset.mems`).
  - Updated `ds_virtualize_affinity()` to parse and enforce `cpuset_cpus` mask during process virtualization.
- **Classification:** High-value / Performance & Efficiency.
- **Validation:** Live test on `mikasa` container verified all 214 tasks restricted to Cores 0–5, isolating Gold cores 6 and 7.

---

### Optimization Entry 003: Memory Swappiness Tuning (`memory.swappiness`)
- **Component:** `src/cgroup/limits.c`, `src/config/`, `src/cli/`
- **Original Behavior:** `memory.swappiness` in the container's memory cgroup defaulted to 60. Under concurrent workloads, the kernel aggressively swapped guest pages out to ZRAM at 6.5–7.7 MB/s, causing `kswapd0` CPU storms and battery drain.
- **New Behavior:** Added `swappiness` config knob (`swappiness=0..100`, CLI `--swappiness=N`) and enforced it via `memory.swappiness` in `ds_cgroup_apply_limits()`.
- **Classification:** Battery-focused / Balanced.
- **Validation:** Verified write of `swappiness=10` on `mikasa` cgroup, reducing anonymous memory swap aggressiveness.

---

### Optimization Entry 004: Automated Stale Socket Scavenger
- **Component:** `src/android/anland/anland.c` (`ds_anland_sweep_stale_sockets`), `src/include/android/android.h`
- **Original Behavior:** Anland display broker created sockets named with random UUIDs (`anland-<uuid>.sock`) in `get_net_dir()`. When containers crashed or shut down uncleanly, sockets were orphaned. 25 sockets accumulated in `/data/local/Droidspaces/Net/`.
- **New Behavior:** Implemented non-blocking connection probing in `ds_anland_sweep_stale_sockets()`. Called on daemon startup and shutdown to identify and unlink dead unreferenced sockets (`ECONNREFUSED` / `ENOENT`) while preserving active sockets.
- **Classification:** Reliability-focused / Architectural.
- **Validation:** Purged 24 dead sockets from `/data/local/Droidspaces/Net/`, perfectly preserved active socket for container `mikasa`.
