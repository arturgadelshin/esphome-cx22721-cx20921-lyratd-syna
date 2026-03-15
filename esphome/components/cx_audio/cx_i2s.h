#pragma once

#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/speaker/speaker.h"
#include "cx_audio.h"
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace esphome {
namespace cx_i2s {

class CXI2SMicrophone : public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;
  void loop() override {}
  void set_cx_audio(cx_audio::CXAudio *parent) { this->parent_ = parent; }
  bool is_running() const { return this->state_ == microphone::STATE_RUNNING; }
  void publish_data(const std::vector<uint8_t> &data);
  bool set_mic_gain(float mic_gain);
  float get_mic_gain_value() const { return mic_gain_; }
  float mic_gain() { return this->mic_gain_; }

 protected:
  cx_audio::CXAudio *parent_;
  float mic_gain_{24.0f};

  TaskHandle_t mic_task_handle_{nullptr};
  SemaphoreHandle_t stop_semaphore_{nullptr};
  volatile bool task_running_{false};

  static void mic_task(void *arg);
  void read_loop();
  void flush_buffers();
};

class CXI2SSpeaker : public speaker::Speaker, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;
  void loop() override;
  size_t play(const uint8_t *data, size_t length) override;
  bool has_buffered_data() const override;
  void set_cx_audio(cx_audio::CXAudio *parent) { this->parent_ = parent; }

 protected:
  cx_audio::CXAudio *parent_;
  std::vector<uint8_t> partial_buffer_;
};

}  // namespace cx_i2s
}  // namespace esphome
