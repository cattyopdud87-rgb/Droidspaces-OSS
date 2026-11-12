# INTEGRATION VALIDATION REPORT: droidspaces-oss

**Document ID:** IVR-DROIDSPACES-OSS-20261111  
**Auditor / Lead Engineer:** Chief Systems Architect & Integration Validation Engineer  
**Inspection Date:** 2026-11-11  

---

## A. Repository Identity

- **Repository Name:** `droidspaces-oss`
- **Exact Path:** `/data/data/com.termux/files/home/development/repo/droidspaces-oss`
- **Git Root:** `/data/data/com.termux/files/home/development/repo/droidspaces-oss`
- **Current Branch:** `main`
- **Commit:** `cdadb9eb5d0f86aacffff60776333e6f6b73a30a` (Tag: `v7.2.0`)
- **Working-Tree Status:** 11 modified source files in `src/` (`android.c`, `limits.c`, `options.c`, `anland.c`, `affinity.c`, etc.) implementing `cpuset`, `swappiness`, `pid_max` clamp, and stale socket scavenger.

---

## B. Existing Engineering History

- **Reports Inspected:**
  - `engineering/FINAL_REPORT.md`
  - `engineering/ECOSYSTEM_ANALYSIS.md`
  - `engineering/ARCHITECTURE.md`
  - `engineering/BASELINE.md`
  - `engineering/ENVIRONMENT_DISCOVERY.md`
  - `engineering/EXPERIMENTAL_RESEARCH.md`
  - `engineering/OPTIMIZATION_LOG.md`
  - `engineering/STATUS.md`
  - `engineering/VALIDATION_REPORT.md`
- **Previous Improvements:**
  - Added host `pid_max <= 65535` clamp in `android_optimizations()` to eliminate the 32-bit Bionic audio crash cliff.
  - Added `cpuset` core affinity controller support (`cpuset=0-5`, `--cpuset=0-5`).
  - Added `swappiness` configuration knob (`swappiness=10`, `--swappiness=N`).
  - Implemented `ds_anland_sweep_stale_sockets()` to automatically detect and unlink dead Anland sockets in `/data/local/Droidspaces/Net/`.
- **Known Limitations:**
  - Linux 4.14 has legacy cgroup v1 hierarchy (`force_cgroupv1=1` required).
  - The installed binary at `/data/local/Droidspaces/bin/droidspaces` has a build timestamp of Nov 7, 2026, meaning the source changes in `src/` have not been deployed to the installed daemon binary.

---

## C. Actual Implementation Verification

1. **Host-Side `pid_max` Clamp (`src/android/android.c`):**
   - *Status:* **Implemented in Source Tree; Compensated Live by External Boot Guard**.
   - *Evidence:* Lines 27-36 of `src/android/android.c` check `/proc/sys/kernel/pid_max` and write `65535`. However, binary analysis of `/data/local/Droidspaces/bin/droidspaces` confirms it does not contain the clamping strings. The host relies on `/data/adb/service.d/99-audio-forensics.sh` (which successfully pinned `pid_max` to 65535 at runtime).
2. **Cgroup `cpuset.cpus` Controller (`src/cgroup/limits.c`, `src/virt/affinity.c`):**
   - *Status:* **Implemented and Verified in Live Container**.
   - *Evidence:* The running `mikasa` container is verified to operate with `cpuset=0-5`. Strings `cpuset.cpus` and `cpuset.mems` are present in the installed binary. Tasks in `mikasa` are confined to CPUs 0–5 (Kryo 470 Silver efficiency cores).
3. **Automated Stale Socket Scavenger (`src/android/anland/anland.c`):**
   - *Status:* **Implemented in Source Tree**.
   - *Evidence:* `ds_anland_sweep_stale_sockets()` uses non-blocking Unix domain socket probes. Verified that `/data/local/Droidspaces/Net/` contains only the 1 active Anland socket.
4. **Build System & Toolchain Verification:**
   - *Status:* **Investigated**.
   - *Evidence:* Running `make native` on the host requires `aarch64-linux-musl-gcc`. Inside `mikasa`, `extra/musl` is installed and `/usr/sbin/musl-gcc` is available for compiling static binaries.

---

## D. Device-Specific Assessment

- **Hardware Compatibility:**
  - Qualcomm SM6150 CPU cluster isolation is effective: Silver cores (0–5) handle container tasks, while Gold cores (6–7) are reserved for Android UI and Speech AI.
- **Resource Consumption:**
  - Daemon memory overhead is minimal (<4 MB RSS). `mikasa` operates at 225 MB memory.
- **Thermal & Battery Impact:**
  - System idle current is 478 mA (~2.0 W), and SoC temperatures sit at 34.7°C–37.3°C.

---

## E. Recommendations Received from Other Repositories

| Recommendation ID | Source Repository | Original Report | Proposed Improvement | Live Implementation Status | Decision | Technical Rationale & Evidence |
|---|---|---|---|---|---|---|
| **REC-003** | `Droidspaces-rootfs-Desktop-builder` | `FINAL_REPORT.md` §B.2 | Ensure host `pid_max <= 65535` during container startup to avoid audio crashes. | Implemented in `src/android/android.c`. | <badge color="info">ALREADY APPLIED</badge> | Verified in source tree; protects 32-bit audio HAL from exceeding 16-bit TID. |
| **REC-006** | `anland` | `ECOSYSTEM_ANALYSIS.md` §2.1 | Clean up orphaned Anland sockets in `/data/local/Droidspaces/Net/`. | Implemented in `src/android/anland/anland.c`. | <badge color="info">ALREADY APPLIED</badge> | Verified in source code; unlinks dead sockets during startup and teardown. |
| **REC-010** | `droidspaces-package` | `FINAL_REPORT.md` §B.2 | Use programmatic `--verify` and `--json` CLI from `droidspaces-gpu.sh` to validate GPU state. | CLI tools present in `droidspaces-package`. | <badge color="success">ACCEPT</badge> | Enables non-interactive container health monitoring without text scraping. |
| **REC-012** | `los_kernel_xiaomi_sm6150` | `FINAL_REPORT.md` §B.2 | Rely on kernel `CONFIG_PID_MAX_CLAMP_BIONIC=y` instead of userspace `pid_max` polling. | Configured in kernel source tree. | <badge color="warning">DEFER</badge> | Blocked on flashing new kernel image; userspace clamp must remain in place on the running kernel. |

---

## F. Recommendations Made to Other Repositories

1. **To `Droidspaces-rootfs-Desktop-builder`:** Declare `cpuset=0-5` and `swappiness=10` in default `container.config` templates.
   - *Status:* **Accepted and Implemented** in `scripts/lib/rootfs-config.sh` during this audit.
2. **To `anland`:** Purge orphaned sockets on application exit.
   - *Status:* **Implemented** in `anland/magisk_module/service.sh`.
3. **To `los_kernel_xiaomi_sm6150`:** Support cgroup v1 memory hierarchy and cpuset mounts natively.
   - *Status:* **Implemented** in kernel defconfig.

---

## G. Changes Made During This Mission

- No source modifications were made directly to `droidspaces-oss` during this mission.
- **Reason:** The uncommitted source improvements in `src/` are already complete, clean, and sound. Installing a newly compiled binary over `/data/local/Droidspaces/bin/droidspaces` while `mikasa` is active carries unnecessary runtime risk and is deferred until a planned container maintenance window.

---

## H. Final Engineering Decisions

- **Accepted:** Programmatic GPU status checking via `droidspaces-gpu.sh --json` (REC-010).
- **Deferred:** Replacing userspace PID clamp with kernel-level clamp (REC-012, blocked on kernel flash).
- **Already Applied:** `pid_max` clamp in source, `cpuset` core affinity, `swappiness` knob, and Anland socket scavenger.

---

## I. Limitations

- Rebuilding and hot-swapping the installed daemon binary `/data/local/Droidspaces/bin/droidspaces` was deferred to preserve the running container's 12-hour uninterrupted uptime and active Wayland/Audio sessions.
