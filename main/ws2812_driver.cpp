/* Created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com
 *
 * Uses the RMT peripheral on the ESP32 for very accurate timing of
 * signals sent to the WS2812 LEDs.
 *
 * This code is placed in the public domain (or CC0 licensed, at your option).
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_task_wdt.h>
#include <esp_intr_alloc.h>
#include <esp_clk_tree.h>

#include <soc/gpio_sig_map.h>
#include <soc/rmt_struct.h>
#include <soc/dport_reg.h>
#include <soc/rtc.h>

#include <driver/gpio.h>
#include <driver/rmt.h>

#include "ws2812_driver.h"
#include "app_utils.h"

/* ********************************
 *  Manufacturer Specification  *
 * ---------------------------- *
 *   T0H 0 time 0.40us ±150ns   *
 *   T0L 0 time 0.85us ±150ns   *
 *   T1H 1 time 0.80us ±150ns   *
 *   T1L 1 time 0.45us ±150ns   *
 *   RES low voltage >= 50μs    *
 ********************************/

// -------------------------------------------------------------------
// Optimized settings using logic probe
// It seems the "high" part of the rmt output is consistently longer
// than the "low" for the same value (see T0L & T1H below).
// -------------------------------------------------------------------

/* *********************************************************************
 * After analysing the various LEDs in the chain...
 * I think the only crucial timing is T0H.
 * On all LEDs, it is amazingly consistent at 250ns high, regardless
 * of the timing we send, whereas all the other timings can vary
 * quite considerably. Due to the variation of the built-in clock of
 * the ESP32, the lowest we can go is 350ns for T0H.
 *********************************************************************/
#define WS2812_T0H 0.00000035
#define WS2812_T0L 0.00000080
#define WS2812_T1H 0.00000080
#define WS2812_T1L 0.00000040
#define WS2812_RES 0.00005000

// ---------------------------------------
// Globals
// ---------------------------------------
WORD_ALIGNED_ATTR DRAM_ATTR static bool initialised         = false;
WORD_ALIGNED_ATTR DRAM_ATTR static uint8_t* ws2812_buffer   = NULL;
WORD_ALIGNED_ATTR DRAM_ATTR static uint16_t ws2812_buflen   = 0;

WORD_ALIGNED_ATTR DRAM_ATTR static rmt_item32_t ws2812_bit0 = {};
WORD_ALIGNED_ATTR DRAM_ATTR static rmt_item32_t ws2812_bit1 = {};
WORD_ALIGNED_ATTR DRAM_ATTR static rmt_item32_t ws2812_res  = {};

// ---------------------------------------
// Declarations
// ---------------------------------------
uint16_t set_ws2812_bits (uint16_t num_leds);
uint16_t led_debug_cnt   = 0;
uint64_t led_write_start = 0;
uint64_t led_write_last  = 0;
uint32_t led_write_count = 0;
uint64_t led_accum_time  = 0;

// ---------------------------------------
// Constants used by either method
// ---------------------------------------
#define LED_RMT_TX_CHANNEL      RMT_CHANNEL_0
#define LED_STRIP_RMT_CLK_DIV   4
#define BITS_PER_LED_CMD        24
#define RMT_TICKS(us)           (uint32_t)((us / LED_STRIP_RMT_CLK_DIV) * APB_CLK_FREQ)

// ***********************************************************************************************************************************
#if defined (CONFIG_USE_RMT_INTERRUPT)
// ***********************************************************************************************************************************
#define MAX_PULSES              32

WORD_ALIGNED_ATTR DRAM_ATTR static uint16_t ws2812_pos  = 0;
WORD_ALIGNED_ATTR DRAM_ATTR static uint16_t ws2812_half = 0;
WORD_ALIGNED_ATTR DRAM_ATTR static uint16_t ws2812_len  = 0;

WORD_ALIGNED_ATTR DRAM_ATTR static SemaphoreHandle_t ws2812_sem  = NULL;
WORD_ALIGNED_ATTR DRAM_ATTR static intr_handle_t rmt_intr_handle = NULL;

IRAM_ATTR void ws2812_copy ()
{
  unsigned int i, j, offset, len, bit;

  offset      = ws2812_half * MAX_PULSES;
  ws2812_half = !ws2812_half;
  len         = ws2812_len - ws2812_pos;

  if ( len > (MAX_PULSES / 8) )
  {
    len = (MAX_PULSES / 8);
  }

  if ( !len )
  {
    for ( i = 0; i < MAX_PULSES; i++ )
    {
      RMTMEM.chan[LED_RMT_TX_CHANNEL].data32[i + offset].val = 0;
    }

    return;
  }

  for ( i = 0; i < len; i++ )
  {
    bit = ws2812_buffer[i + ws2812_pos];
    for ( j = 0; j < 8; j++, bit <<= 1 )
    {
      RMTMEM.chan[LED_RMT_TX_CHANNEL].data32[j + i * 8 + offset].val = ((bit >> 7) & 0x01)?ws2812_bit1.val:ws2812_bit0.val;
    }

    if ( i + ws2812_pos == ws2812_len - 1 )
    {
      RMTMEM.chan[LED_RMT_TX_CHANNEL].data32[7 + i * 8 + offset].duration1 = ws2812_res.duration0;
    }
  }

  for ( i *= 8; i < MAX_PULSES; i++ )
  {
    RMTMEM.chan[LED_RMT_TX_CHANNEL].data32[i + offset].val = 0;
  }

  ws2812_pos += len;

  led_write_last = esp_timer_get_time ();
  led_debug_cnt++;

  return;
}

IRAM_ATTR void ws2812_handleInterrupt (void* arg)
{
  static BaseType_t xHigherPriorityTaskWoken;

  if ( RMT.int_st.ch0_tx_thr_event )
  {
    ws2812_copy ();
    RMT.int_clr.ch0_tx_thr_event = 1;
  }
  else if ( RMT.int_st.ch0_tx_end && ws2812_sem )
  {
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR (ws2812_sem, &xHigherPriorityTaskWoken);
    RMT.int_clr.ch0_tx_end = 1;
  }
  portYIELD_FROM_ISR ( xHigherPriorityTaskWoken );

  return;
}

IRAM_ATTR esp_err_t ws2812_setColors (uint16_t num_leds, cRGB* array)
{
  if ( led_write_start )
  {
    led_accum_time += (led_write_last - led_write_start);
  }
  led_write_count++;
  led_write_start = esp_timer_get_time ();
  led_debug_cnt = 0;

  if ( ws2812_buffer != NULL && num_leds <= ws2812_buflen )
  {
    // taskENTER_CRITICAL(&myMutex);
    vTaskSuspendAll ();

    // Wait for previous operation to finish
    xSemaphoreTake (ws2812_sem, (TickType_t)0);

    ws2812_len = (num_leds * sizeof (cRGB));
    memcpy (ws2812_buffer, array, ws2812_len);

    ws2812_pos = 0;
    ws2812_half = 0;

    ws2812_copy ();

    if ( ws2812_pos < ws2812_len )
    {
      ws2812_copy ();
    }

    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.mem_rd_rst = 1;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.tx_start = 1;

    // Wait for operation to finish
    if ( xSemaphoreTake (ws2812_sem, 0) == pdTRUE )
    {
      xSemaphoreTake (ws2812_sem, portMAX_DELAY);
    }

    xSemaphoreGive (ws2812_sem);

    xTaskResumeAll ();
    // taskEXIT_CRITICAL(&myMutex);
  }

  return ESP_OK;
}

int ws2812_init (gpio_num_t gpioNum, uint16_t num_leds)
{
  esp_err_t err = ESP_FAIL;

  if ( !initialised )
  {
    if ( ws2812_sem == NULL )
    {
      ws2812_sem = xSemaphoreCreateBinary ();
      assert (ws2812_sem != NULL);

      if ( ws2812_sem == NULL )
      {
        return ESP_FAIL;
      }

      xSemaphoreGive (ws2812_sem);

      // Enable SPI
      DPORT_SET_PERI_REG_MASK (DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
      DPORT_CLEAR_PERI_REG_MASK (DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);
    }

    // Allocate RMT interrupt
    if ( rmt_intr_handle == NULL )
    {
      err = esp_intr_alloc (ETS_RMT_INTR_SOURCE, 0, ws2812_handleInterrupt, NULL, &rmt_intr_handle);
      if (err != ESP_OK)
        return err;
    }

    // Allocate our local storage buffer
    if ( ws2812_buffer == NULL )
    {
      ws2812_buflen = (num_leds * 3) * sizeof (uint8_t);
      ws2812_buffer = (uint8_t *)heap_caps_malloc(ws2812_buflen, MALLOC_CAP_DMA);
      if ( ws2812_buffer == NULL )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "heap_caps_malloc failed allocating %d bytes for 'ws2812_buffer'", ws2812_buflen);
      }
    }

    xSemaphoreTake (ws2812_sem, portMAX_DELAY);

    err = rmt_set_gpio ((rmt_channel_t)LED_RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t)gpioNum, false);
    // err = rmt_set_pin((rmt_channel_t) LED_RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t) gpioNum);
    if ( err != ESP_OK )
    {
      xSemaphoreGive (ws2812_sem);
      return err;
    }

    RMT.apb_conf.fifo_mask = 1;
    RMT.apb_conf.mem_tx_wrap_en = 1;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf0.div_cnt = LED_STRIP_RMT_CLK_DIV;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf0.mem_size = 1;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf0.carrier_en = 0;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf0.carrier_out_lv = 1;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf0.mem_pd = 0;

    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.rx_en = 0;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.mem_owner = 0;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.tx_conti_mode = 0; // loop back mode.
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.ref_always_on = 1; // use apb clock: 80M
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.idle_out_en = 1;
    RMT.conf_ch[LED_RMT_TX_CHANNEL].conf1.idle_out_lv = 0;

    RMT.tx_lim_ch[LED_RMT_TX_CHANNEL].limit = MAX_PULSES;
    // RMT.int_ena.val = tx_thr_event_mask | tx_end_event_mask;
    RMT.int_ena.ch0_tx_thr_event = 1;
    RMT.int_ena.ch0_tx_end = 1;

    set_ws2812_bits (num_leds);

    xSemaphoreGive (ws2812_sem);

    initialised = true;
  }
  return err;
}

void ws2812_end (void)
{
  xSemaphoreTake (ws2812_sem, portMAX_DELAY);

  if ( initialised )
  {
    if ( ws2812_buffer )
    {
      heap_caps_free(ws2812_buffer);
      ws2812_buffer = NULL;
    }
    initialised = false;
  }

  xSemaphoreGive (ws2812_sem);
  vSemaphoreDelete (ws2812_sem);
  ws2812_sem = NULL;
}
// ***********************************************************************************************************************************
#else  // End of CONFIG_USE_RMT_INTERRUPT
// ***********************************************************************************************************************************
IRAM_ATTR static void ws2812_rmt_adapter (const void* src, rmt_item32_t* dest, size_t src_size, size_t wanted_num, size_t* translated_size, size_t* item_num)
{
  *translated_size = 0;
  *item_num        = 0;

  if ( src && dest )
  {
    uint8_t* psrc = (uint8_t*)src;
    rmt_item32_t* pdest = dest;

    while ( *translated_size < src_size && *item_num < wanted_num )
    {
      for ( int i = 0; i < 8; i++ )
      {
        // MSB first1
        (pdest++)->val = *psrc & (1 << (7 - i))?ws2812_bit1.val:ws2812_bit0.val;
        *item_num = *item_num + 1;
      }
      *translated_size = *translated_size + 1;
      psrc++;
    }
  }

  led_write_last = esp_timer_get_time ();
  led_debug_cnt++;
}

IRAM_ATTR esp_err_t ws2812_setColors (uint16_t num_leds, cRGB* array)
{
  esp_err_t err = ESP_OK;

  if ( led_write_start )
  {
    led_accum_time += (led_write_last - led_write_start);
  }
  led_write_count++;
  led_write_start = esp_timer_get_time ();
  led_debug_cnt = 0;

  memcpy (ws2812_buffer, array, (num_leds * sizeof (cRGB)));

  err = rmt_wait_tx_done (LED_RMT_TX_CHANNEL, portMAX_DELAY);
  if ( err != ESP_OK )
  {
    switch (err)
    {
      case ESP_ERR_TIMEOUT:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_wait_tx_done timeout waiting");
        break;
      case ESP_ERR_INVALID_ARG:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_wait_tx_done parameter error");
        break;
      case ESP_FAIL:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_wait_tx_done driver not installed");
        break;
      default:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_wait_tx_done unknown error: %d", err);
    }
  }

  err = rmt_write_sample (LED_RMT_TX_CHANNEL, ws2812_buffer, (num_leds * sizeof (cRGB)), false);
  if ( err != ESP_OK )
  {
    switch (err)
    {
      case ESP_ERR_INVALID_ARG:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_write_items parameter error");
        break;
      default:
        F_LOGE(true, true, LC_BRIGHT_RED, "rmt_write_items unknown error: %d", err);
    }
  }

  return err;
}

esp_err_t ws2812_init (gpio_num_t gpioNum, uint16_t num_leds)
{
  esp_err_t err = ESP_OK;

  if ( !initialised )
  {
    rmt_config_t config  = RMT_DEFAULT_CONFIG_TX (gpioNum, LED_RMT_TX_CHANNEL);
    config.mem_block_num = 1;
    config.clk_div       = LED_STRIP_RMT_CLK_DIV;

    rmt_config (&config);
    rmt_driver_install (config.channel, 0, 0);

    uint32_t clock_hz = 0;
    rmt_get_counter_clock (config.channel, &clock_hz);
    F_LOGI(true, true, LC_GREY, "Clock Hz = %d", clock_hz);

    set_ws2812_bits (num_leds);

    rmt_translator_init (config.channel, ws2812_rmt_adapter);

    // Allocate our local storage buffer
    if ( ws2812_buffer == NULL )
    {
      ws2812_buflen = (num_leds * sizeof (cRGB));
      ws2812_buffer = (uint8_t *)heap_caps_malloc(ws2812_buflen, MALLOC_CAP_DMA);
      if ( ws2812_buffer == NULL )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "heap_caps_malloc failed allocating %d bytes for 'ws2812_buffer'", ws2812_buflen);
      }
    }

    initialised = true;
  }

  return err;
}

esp_err_t ws2812_end (void)
{
  esp_err_t err = ESP_OK;

  if (initialised)
  {
    heap_caps_free(ws2812_buffer);
    ws2812_buffer = 0;
    initialised = false;
  }

  return err;
}
// ***********************************************************************************************************************************
#endif // End of USE_INTERRUPT
// ***********************************************************************************************************************************
uint16_t set_ws2812_bits (uint16_t num_leds)
{
  //ws2812_bit0 = (rmt_item32_t){{{RMT_TICKS(WS2812_T0H), 1, RMT_TICKS(WS2812_T0L), 0}}};
  ws2812_bit0 = (rmt_item32_t){{{RMT_TICKS(WS2812_T0H), 1, RMT_TICKS(WS2812_T0L), 0}}};
  ws2812_bit1 = (rmt_item32_t){{{RMT_TICKS(WS2812_T1H), 1, RMT_TICKS(WS2812_T1L), 0}}};
  ws2812_res  = (rmt_item32_t){{{RMT_TICKS(WS2812_RES), 0, 0, 0}}};

  uint32_t xtal_freq = rtc_clk_xtal_freq_get ();
  uint32_t apb_freq = rtc_clk_apb_freq_get () / 1000000;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 2, 0))
  uint32_t cpu_freq = xt_clock_freq() / 1000000;
#else
  uint32_t cpu_freq = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &cpu_freq);
  cpu_freq = cpu_freq / 1000000;
#endif
  //  apb_freq = ((READ_PERI_REG(RTC_CNTL_STORE5_REG)) & UINT16_MAX) << 12;
  F_LOGI(true, true, LC_GREY, "xtal_freq: %d, apb_freq: %d MHz, cpu_freq: %d MHz", xtal_freq, apb_freq, cpu_freq);

  F_LOGI(true, true, LC_GREY, "T0H = %d, T0L = %d, T1H = %d, T1L = %d", ws2812_bit0.duration0, ws2812_bit0.duration1, ws2812_bit1.duration0, ws2812_bit1.duration1);

  // This seems to result in an accurate(ish) value
  float updateDuration = (((WS2812_T1H + WS2812_T1L) * BITS_PER_LED_CMD) * 1000000) * num_leds;
  F_LOGI(true, true, LC_GREY, "Time to update all %d LEDs is around %d us", num_leds, (uint16_t)updateDuration);

  return (uint16_t)updateDuration;
}
