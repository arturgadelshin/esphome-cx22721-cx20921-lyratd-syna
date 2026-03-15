#include "cx_i2s.h"
#include "cx_audio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>

extern "C" {
#include <va_dsp.h>
#include <cnx20921_init.h>
int cx20921SetMicGain(int gain_db);
}

namespace esphome {
namespace cx_i2s {

static const char *const TAG = "cx_i2s";

extern "C" void esphome_set_dsp_fw_mode(int mode);

void CXI2SMicrophone::setup() {
  ESP_LOGI(TAG, "Setting up CX I2S Microphone...");

  bool use_fw = this->parent_->is_use_firmware();
  int fw_mode = use_fw ? 1 : 0;
  ESP_LOGI(TAG, "DSP Init Config: use_firmware=%s, mode=%d", use_fw ? "YES" : "NO", fw_mode);

  esphome_set_dsp_fw_mode(fw_mode);

  ESP_LOGI(TAG, "Performing hardware reset on GPIO21...");
  gpio_num_t reset_pin = GPIO_NUM_21;
  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << reset_pin),
                           .mode = GPIO_MODE_OUTPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);
  gpio_set_level(reset_pin, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(reset_pin, 1);
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_LOGI(TAG, "Calling va_dsp_init...");
  va_dsp_init(nullptr, nullptr, nullptr);
  ESP_LOGI(TAG, "va_dsp_init returned");

  if (this->mic_gain_ != 0.0f) {
    cx20921SetMicGain((int) this->mic_gain_);
  }

  this->stop_semaphore_ = xSemaphoreCreateBinary();
}

void CXI2SMicrophone::start() {
  if (this->task_running_) {
    ESP_LOGW(TAG, "Microphone task already running");
    return;
  }

  ESP_LOGI(TAG, "Starting microphone task...");

  // this->flush_buffers(); // УБРАТЬ: this->flush_buffers();

  this->task_running_ = true;
  this->state_ = microphone::STATE_RUNNING;

  BaseType_t ret = xTaskCreatePinnedToCore(mic_task, "cx_mic_task", 8192, this, 5, &this->mic_task_handle_, 1);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create microphone task!");
    this->state_ = microphone::STATE_STOPPED;
    this->task_running_ = false;
    return;
  }

  ESP_LOGI(TAG, "Microphone task started on core 1");
}

void CXI2SMicrophone::stop() {
  if (!this->task_running_) {
    this->state_ = microphone::STATE_STOPPED;
    return;
  }

  ESP_LOGI(TAG, "Stopping microphone task...");

  this->task_running_ = false;

  if (this->stop_semaphore_ != nullptr) {
    if (xSemaphoreTake(this->stop_semaphore_, pdMS_TO_TICKS(500)) != pdTRUE) {
      ESP_LOGW(TAG, "Timeout waiting for task to stop");
    }
  }

  this->mic_task_handle_ = nullptr;
  this->state_ = microphone::STATE_STOPPED;
  ESP_LOGI(TAG, "Microphone stopped");
}

void CXI2SMicrophone::flush_buffers() {
  ESP_LOGD(TAG, "Flushing I2S DMA buffers...");

  i2s_zero_dma_buffer(I2S_NUM_1);

  uint8_t discard[640];
  size_t bytes_read;

  for (int i = 0; i < 60; i++) {
    i2s_read(I2S_NUM_1, discard, sizeof(discard), &bytes_read, pdMS_TO_TICKS(10));
  }

  ESP_LOGD(TAG, "Buffer flush complete");
}

void CXI2SMicrophone::mic_task(void *arg) {
  CXI2SMicrophone *self = static_cast<CXI2SMicrophone *>(arg);
  // Flush buffers здесь, а не в start()
  self->flush_buffers();

  ESP_LOGI(TAG, "Mic task running on core %d", xPortGetCoreID());

  while (self->task_running_) {
    self->read_loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  ESP_LOGI(TAG, "Mic task exiting");

  if (self->stop_semaphore_ != nullptr) {
    xSemaphoreGive(self->stop_semaphore_);
  }

  vTaskDelete(nullptr);
}

void CXI2SMicrophone::read_loop() {
  uint8_t stereo_buffer[640];
  size_t bytes_read = 0;

  esp_err_t err = i2s_read(I2S_NUM_1, stereo_buffer, sizeof(stereo_buffer), &bytes_read, pdMS_TO_TICKS(10));

  if (err != ESP_OK || bytes_read == 0) {
    return;
  }

  size_t samples = bytes_read / 4;
  std::vector<uint8_t> mono_data;
  mono_data.reserve(samples * 2);

  for (size_t i = 0; i < samples; i++) {
    mono_data.push_back(stereo_buffer[i * 4 + 0]);
    mono_data.push_back(stereo_buffer[i * 4 + 1]);
  }

  if (!mono_data.empty()) {
    this->data_callbacks_.call(mono_data);
  }
}

bool CXI2SMicrophone::set_mic_gain(float mic_gain) {
  this->mic_gain_ = clamp<float>(mic_gain, 0.0f, 30.0f);
  if (this->state_ == microphone::STATE_RUNNING) {
    cx20921SetMicGain((int) this->mic_gain_);
  }
  return true;
}

void CXI2SMicrophone::publish_data(const std::vector<uint8_t> &data) { this->data_callbacks_.call(data); }

void CXI2SSpeaker::setup() {}
void CXI2SSpeaker::start() { this->state_ = speaker::STATE_RUNNING; }
void CXI2SSpeaker::stop() { this->state_ = speaker::STATE_STOPPED; }
void CXI2SSpeaker::loop() {}
size_t CXI2SSpeaker::play(const uint8_t *data, size_t length) {
  size_t written = 0;
  i2s_write(I2S_NUM_0, data, length, &written, pdMS_TO_TICKS(10));
  return written;
}
bool CXI2SSpeaker::has_buffered_data() const { return false; }

}  // namespace cx_i2s
}  // namespace esphome
