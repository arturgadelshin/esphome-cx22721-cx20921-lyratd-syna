#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {

static const char *const CPU_STATS_TAG = "cpustats";

class CPUStatsComponent : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }

  void loop() override {
    uint32_t now = millis();
    if (now - last_dump_ < DUMP_INTERVAL_MS) return;

    if (!initialized_) {
      initialized_ = true;
      last_dump_ = now;
      snapshot_();
      ESP_LOGI(CPU_STATS_TAG, "CPU stats initialized, first dump in %ds", DUMP_INTERVAL_MS / 1000);
      return;
    }

    uint32_t interval = now - last_dump_;
    last_dump_ = now;
    dump_(interval);
    snapshot_();
  }

 private:
  static const int MAX_TASKS = 24;
  static const uint32_t DUMP_INTERVAL_MS = 30000;

  void snapshot_() {
    TaskStatus_t tasks[MAX_TASKS];
    uint32_t total;
    prev_count_ = uxTaskGetSystemState(tasks, MAX_TASKS, &total);
    for (UBaseType_t i = 0; i < prev_count_; i++) {
      prev_handles_[i] = tasks[i].xHandle;
      prev_counters_[i] = tasks[i].ulRunTimeCounter;
    }
    prev_total_ = total;
  }

  void dump_(uint32_t interval_ms) {
    TaskStatus_t tasks[MAX_TASKS];
    uint32_t total;
    UBaseType_t count = uxTaskGetSystemState(tasks, MAX_TASKS, &total);

    uint32_t delta_total = total - prev_total_;
    if (delta_total == 0) {
      ESP_LOGW(CPU_STATS_TAG, "No CPU time elapsed since last dump");
      return;
    }

    float core0_load = 0, core1_load = 0, core0_idle = 0, core1_idle = 0;

    ESP_LOGI(CPU_STATS_TAG, "=== CPU LOAD (%.1fs window) ===", interval_ms / 1000.0f);
    ESP_LOGI(CPU_STATS_TAG, "%-16s C Pri   CPU%%   Stk(wm)", "Task");

    for (UBaseType_t i = 0; i < count; i++) {
      uint32_t prev_ctr = 0;
      for (UBaseType_t j = 0; j < prev_count_; j++) {
        if (prev_handles_[j] == tasks[i].xHandle) {
          prev_ctr = prev_counters_[j];
          break;
        }
      }
      uint32_t delta_ctr = tasks[i].ulRunTimeCounter - prev_ctr;
      float pct = (float) ((uint64_t) delta_ctr * 100ULL / delta_total);
      int core = (int) xTaskGetCoreID(tasks[i].xHandle);
      const char *name = tasks[i].pcTaskName;
      bool is_idle = (strncmp(name, "IDLE", 4) == 0);

      if (pct >= 0.1f) {
        ESP_LOGI(CPU_STATS_TAG, "%-16s %d %3d  %5.1f%%  %5u",
                 name, core, tasks[i].uxCurrentPriority,
                 pct, tasks[i].usStackHighWaterMark);
      }

      if (is_idle) {
        if (core == 0) core0_idle += pct;
        else if (core == 1) core1_idle += pct;
      } else {
        if (core == 0) core0_load += pct;
        else if (core == 1) core1_load += pct;
      }
    }

    ESP_LOGI(CPU_STATS_TAG, "------");
    ESP_LOGI(CPU_STATS_TAG, "Core0: load %5.1f%%  idle %5.1f%%  |  Core1: load %5.1f%%  idle %5.1f%%",
             core0_load, core0_idle, core1_load, core1_idle);
  }

  bool initialized_{false};
  uint32_t last_dump_{0};
  TaskHandle_t prev_handles_[MAX_TASKS]{};
  uint32_t prev_counters_[MAX_TASKS]{};
  UBaseType_t prev_count_{0};
  uint32_t prev_total_{0};
  float idle_pct_[2]{0, 0};
};

}  // namespace esphome
