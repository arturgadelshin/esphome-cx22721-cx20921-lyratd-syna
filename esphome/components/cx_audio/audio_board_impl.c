// Реализации функций audio_board и media_hal для LyraTD CNX
// Эти функции должны быть в библиотеке, но не линкуются, поэтому создаем их здесь

#include <esp_log.h>
#include <driver/i2s.h>
#include <audio_board.h>
#include <media_hal.h>
#include <media_hal_playback.h>
#include <media_hal_codec_init.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdlib.h>
#include <string.h>

#define PLAT_TAG "AUDIO_BOARD"
#define HAL_TAG "MEDIA_HAL"

static media_hal_t *media_hal_handle = NULL;

esp_err_t audio_board_i2s_pin_config(int port_num, i2s_pin_config_t *ab_i2s_pin) {
  if (ab_i2s_pin == NULL) {
    ESP_LOGE(PLAT_TAG, "Error assigning i2s pins: ab_i2s_pin is NULL");
    return ESP_FAIL;
  }

  switch (port_num) {
    case 0:
      ab_i2s_pin->bck_io_num = GPIO_NUM_5;
      ab_i2s_pin->ws_io_num = GPIO_NUM_25;
      ab_i2s_pin->data_out_num = GPIO_NUM_26;
      ab_i2s_pin->data_in_num = -1;
      break;
    case 1:
      ab_i2s_pin->bck_io_num = GPIO_NUM_33;
      ab_i2s_pin->ws_io_num = GPIO_NUM_32;
      ab_i2s_pin->data_out_num = -1;
      ab_i2s_pin->data_in_num = GPIO_NUM_35;
      break;
    default:
      ESP_LOGE(PLAT_TAG, "Entered i2s port number is wrong: %d", port_num);
      return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t audio_board_i2s_init_default(i2s_config_t *i2s_cfg_dft) {
  if (i2s_cfg_dft == NULL) {
    ESP_LOGE(PLAT_TAG, "Error initializing i2s config: i2s_cfg_dft is NULL");
    return ESP_FAIL;
  }

  i2s_cfg_dft->mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX;
  i2s_cfg_dft->sample_rate = 16000;  // Было 48000
  i2s_cfg_dft->bits_per_sample = 16;
  i2s_cfg_dft->channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_cfg_dft->communication_format = I2S_COMM_FORMAT_I2S;
  i2s_cfg_dft->dma_buf_count = 8;
  i2s_cfg_dft->dma_buf_len = 1024;
  i2s_cfg_dft->intr_alloc_flags = 0;
  i2s_cfg_dft->use_apll = 0;
  i2s_cfg_dft->tx_desc_auto_clear = true;

  return ESP_OK;
}
