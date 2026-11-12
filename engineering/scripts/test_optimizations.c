/*
 * Droidspaces Optimization Verification Harness
 * Tests:
 * 1. Stale socket scavenger (ds_anland_sweep_stale_sockets)
 * 2. pid_max clamp (android_optimizations)
 * 3. Configuration parsing/saving for cpuset and swappiness
 */

#include "droidspaces.h"
#include <assert.h>

int main(void) {
  printf("[TEST 1] Testing stale socket scavenger...\n");
  ds_anland_sweep_stale_sockets();
  printf("[TEST 1] Scavenger executed cleanly.\n");

  printf("[TEST 2] Testing pid_max safety clamp...\n");
  android_optimizations(1);
  char pbuf[32] = {0};
  if (read_file("/proc/sys/kernel/pid_max", pbuf, sizeof(pbuf)) > 0) {
    long long pval = atoll(pbuf);
    printf("[TEST 2] /proc/sys/kernel/pid_max reads: %lld\n", pval);
    assert(pval <= 65535);
  }

  printf("[TEST 3] Testing config load/save for cpuset and swappiness...\n");
  struct ds_config cfg;
  ds_config_init(&cfg);
  safe_strncpy(cfg.container_name, "test_opt", sizeof(cfg.container_name));
  safe_strncpy(cfg.cpuset_cpus, "0-5", sizeof(cfg.cpuset_cpus));
  safe_strncpy(cfg.cpuset_mems, "0", sizeof(cfg.cpuset_mems));
  cfg.swappiness = 10;

  const char *test_conf = "/tmp/ds_test_opt.config";
  int save_rc = ds_config_save(test_conf, &cfg);
  assert(save_rc == 0);

  struct ds_config loaded;
  ds_config_init(&loaded);
  int load_rc = ds_config_load(test_conf, &loaded);
  assert(load_rc == 0);
  printf("[TEST 3] Loaded cpuset_cpus: '%s'\n", loaded.cpuset_cpus);
  printf("[TEST 3] Loaded cpuset_mems: '%s'\n", loaded.cpuset_mems);
  printf("[TEST 3] Loaded swappiness: %d\n", loaded.swappiness);
  assert(strcmp(loaded.cpuset_cpus, "0-5") == 0);
  assert(strcmp(loaded.cpuset_mems, "0") == 0);
  assert(loaded.swappiness == 10);
  unlink(test_conf);
  ds_config_free(&cfg);
  ds_config_free(&loaded);

  printf("\n[SUCCESS] All optimization harness tests PASSED!\n");
  return 0;
}
