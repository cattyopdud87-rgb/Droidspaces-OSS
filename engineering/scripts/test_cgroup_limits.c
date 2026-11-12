/*
 * Live Cgroup Limits Verification Harness
 * Tests:
 * 1. ds_cgroup_apply_limits with cpuset_cpus="0-5"
 * 2. ds_cgroup_apply_limits with swappiness=10
 * 3. Readback of /dev/cpuset and /dev/memcg
 */

#include "droidspaces.h"
#include <assert.h>

int main(void) {
  printf("[TEST] Testing live cgroup limit enforcement on mikasa...\n");
  struct ds_config cfg;
  ds_config_init(&cfg);
  safe_strncpy(cfg.container_name, "mikasa", sizeof(cfg.container_name));
  safe_strncpy(cfg.cpuset_cpus, "0-5", sizeof(cfg.cpuset_cpus));
  safe_strncpy(cfg.cpuset_mems, "0", sizeof(cfg.cpuset_mems));
  cfg.swappiness = 10;

  ds_cgroup_apply_limits(&cfg);

  char cpus_buf[64] = {0};
  char swap_buf[32] = {0};

  /* Check /dev/cpuset/droidspaces/mikasa/cpus (Android v1 noprefix) */
  if (read_file("/dev/cpuset/droidspaces/mikasa/cpus", cpus_buf, sizeof(cpus_buf)) > 0) {
    cpus_buf[strcspn(cpus_buf, "\r\n")] = '\0';
    printf("[TEST] /dev/cpuset/droidspaces/mikasa/cpus = '%s'\n", cpus_buf);
    assert(strcmp(cpus_buf, "0-5") == 0);
  } else {
    printf("[FAIL] Failed to read cpuset cpus!\n");
    return 1;
  }

  /* Check /dev/memcg/droidspaces/mikasa/memory.swappiness */
  if (read_file("/dev/memcg/droidspaces/mikasa/memory.swappiness", swap_buf, sizeof(swap_buf)) > 0) {
    int sw = atoi(swap_buf);
    printf("[TEST] /dev/memcg/droidspaces/mikasa/memory.swappiness = %d\n", sw);
    assert(sw == 10);
  } else {
    printf("[FAIL] Failed to read memory.swappiness!\n");
    return 1;
  }

  printf("\n[SUCCESS] Live cgroup limit enforcement PASSED!\n");
  ds_config_free(&cfg);
  return 0;
}
