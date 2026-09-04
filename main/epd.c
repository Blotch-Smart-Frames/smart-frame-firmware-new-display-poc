#include "epd.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "EPD";

static spi_device_handle_t s_spi;

// Panel is specified for a several-second full refresh; give generous
// headroom for cold temperatures so a dead/disconnected panel can't hang the
// task forever.
static const uint32_t EPD_BUSY_TIMEOUT_MS = 30000;

static inline void epd_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void epd_hw_init(void) {
  gpio_config_t out_cfg = {
      .pin_bit_mask = (1ULL << EPD_PIN_CS) | (1ULL << EPD_PIN_DC) |
                      (1ULL << EPD_PIN_RST) | (1ULL << EPD_PIN_EN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&out_cfg);

  gpio_config_t busy_cfg = {
      .pin_bit_mask = (1ULL << EPD_PIN_BUSY),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&busy_cfg);

  gpio_set_level(EPD_PIN_CS, 1);
  gpio_set_level(EPD_PIN_DC, 0);
  gpio_set_level(EPD_PIN_RST, 1);
  gpio_set_level(EPD_PIN_EN, 0);  // Panel powered off until EPD_Reset().

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = EPD_PIN_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = EPD_PIN_SCLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      // Whole frame buffer sent as a single DMA transaction, matching the
      // proven approach used for the existing 13.3" panel driver.
      .max_transfer_sz = EPD_BUFFER_SIZE,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = 4 * 1000 * 1000,
      .mode = 0,
      .spics_io_num = -1,  // CS toggled manually alongside DC, like the sample.
      .queue_size = 1,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi));

  ESP_LOGI(TAG, "GPIO + SPI initialized");
}

// A single SPI transaction on the ESP32-S3's GP-SPI peripheral is hardware-
// limited to well under EPD_BUFFER_SIZE regardless of the max_transfer_sz
// configured at bus-init time (that only bounds DMA descriptor allocation,
// not one transaction's length). Bulk writes are chunked here; CS stays
// asserted across all chunks since the caller toggles it once for the whole
// buffer.
#define EPD_SPI_MAX_CHUNK_BYTES 4092

static void epd_spi_write(const uint8_t *data, size_t len) {
  while (len > 0) {
    size_t chunk = len > EPD_SPI_MAX_CHUNK_BYTES ? EPD_SPI_MAX_CHUNK_BYTES : len;
    spi_transaction_t t = {
        .length = chunk * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
    data += chunk;
    len -= chunk;
  }
}

static void EPD_WriteCMD(uint8_t command) {
  gpio_set_level(EPD_PIN_DC, 0);  // Command mode.
  gpio_set_level(EPD_PIN_CS, 0);
  epd_spi_write(&command, 1);
  gpio_set_level(EPD_PIN_CS, 1);
}

static void EPD_WriteDATA(uint8_t data) {
  gpio_set_level(EPD_PIN_DC, 1);  // Data mode.
  gpio_set_level(EPD_PIN_CS, 0);
  epd_spi_write(&data, 1);
  gpio_set_level(EPD_PIN_CS, 1);
}

static void EPD_WriteDataBuffer(const uint8_t *data, size_t len) {
  gpio_set_level(EPD_PIN_DC, 1);  // Data mode.
  gpio_set_level(EPD_PIN_CS, 0);
  epd_spi_write(data, len);
  gpio_set_level(EPD_PIN_CS, 1);
}

// Ported from the sample's lcd_chkstatus(): BUSY reads 1 while busy, 0 when
// ready. A timeout guard (absent from the raw sample) is added so a dead or
// disconnected panel can't wedge app_main() forever.
static void lcd_chkstatus(void) {
  TickType_t start = xTaskGetTickCount();
  while (gpio_get_level(EPD_PIN_BUSY) != 0) {
    if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(EPD_BUSY_TIMEOUT_MS)) {
      ESP_LOGE(TAG, "Timeout waiting for BUSY to release after %lu ms",
               (unsigned long)EPD_BUSY_TIMEOUT_MS);
      return;
    }
    epd_delay_ms(10);
  }
  epd_delay_ms(100);
}

void EPD_Reset(void) {
  // Power on the panel. The raw vendor sample doesn't have this step because
  // it doesn't use a separate power-enable line; it's added here because the
  // reused pin mapping includes one.
  gpio_set_level(EPD_PIN_EN, 1);
  epd_delay_ms(10);

  gpio_set_level(EPD_PIN_RST, 1);
  epd_delay_ms(100);
  gpio_set_level(EPD_PIN_RST, 0);  // Module reset.
  epd_delay_ms(10);
  gpio_set_level(EPD_PIN_RST, 1);
  epd_delay_ms(100);
  lcd_chkstatus();
  EPD_WriteCMD(0x12);
  epd_delay_ms(100);
}

void EPD_init(void) {
  EPD_WriteCMD(0x01);
  EPD_WriteDATA(0xA7);
  EPD_WriteDATA(0x02);
  EPD_WriteDATA(0x00);

  EPD_WriteCMD(0x11);
  EPD_WriteDATA(0x00);

  EPD_WriteCMD(0x44);
  EPD_WriteDATA(0xBF);
  EPD_WriteDATA(0x03);
  EPD_WriteDATA(0x00);
  EPD_WriteDATA(0x00);

  EPD_WriteCMD(0x45);
  EPD_WriteDATA(0xA7);
  EPD_WriteDATA(0x02);
  EPD_WriteDATA(0x00);
  EPD_WriteDATA(0x00);

  EPD_WriteCMD(0x4E);
  EPD_WriteDATA(0xBF);
  EPD_WriteDATA(0x03);

  EPD_WriteCMD(0x4F);
  EPD_WriteDATA(0xA7);
  EPD_WriteDATA(0x02);

  EPD_WriteCMD(0x3C);  // Border waveform.
  EPD_WriteDATA(0x01);

  EPD_WriteCMD(0x18);  // Temperature sensor: internal.
  EPD_WriteDATA(0x80);

  EPD_WriteCMD(0x0C);  // Booster soft-start.
  EPD_WriteDATA(0xAE);
  EPD_WriteDATA(0xC7);
  EPD_WriteDATA(0xC3);
  EPD_WriteDATA(0xC0);
  EPD_WriteDATA(0x80);
}

static void EPD_FullRefresh(void) {
  EPD_WriteCMD(0x22);
  EPD_WriteDATA(0xF7);
  EPD_WriteCMD(0x20);
  lcd_chkstatus();
}

void EPD_DisplayFull(const uint8_t *image) {
  EPD_WriteCMD(0x24);  // Write B/W RAM.
  EPD_WriteDataBuffer(image, EPD_BUFFER_SIZE);
  EPD_FullRefresh();
}

// Ported from epd.cpp's EPD::sleep(): never cut power while a waveform is
// still running, so guard with a busy-wait even though callers should
// already be idle after EPD_DisplayFull()'s own wait.
void EPD_Sleep(void) {
  lcd_chkstatus();
  EPD_WriteCMD(0x10);  // Enter deep sleep.
  EPD_WriteDATA(0x01);
  epd_delay_ms(100);
  gpio_set_level(EPD_PIN_EN, 0);  // Power down the panel.
}
