/**
 * @file epd.h
 * @brief Minimal ESP32-S3 / ESP-IDF port of the manufacturer's sample driver
 *        for the new 960x680 e-paper panel.
 *
 * Ported 1:1 from the vendor sample (STM32 HAL style: EPD_W21_WriteCMD /
 * EPD_W21_WriteDATA / EPD_Reset / EPD_init / DisplayBW / EPD_FullRefresh),
 * using ESP-IDF's hardware SPI (spi_master) instead of the STM32 bit-bang
 * SPI, and the pin mapping already used for the 13.3" panel in the
 * smart-frame-firmware project.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

// Pin mapping reused from smart-frame-firmware's existing 13.3" EPD wiring.
#define EPD_PIN_MOSI 11
#define EPD_PIN_SCLK 12
#define EPD_PIN_CS 10
#define EPD_PIN_DC 13
#define EPD_PIN_RST 14
#define EPD_PIN_BUSY 21
#define EPD_PIN_EN 15  // Panel power enable.

#define EPD_WIDTH 960
#define EPD_HEIGHT 680
#define EPD_BUFFER_SIZE ((EPD_WIDTH / 8) * EPD_HEIGHT)  // 81600 bytes

// Sets up GPIOs and the SPI bus/device. Must be called once before any other
// epd_* function.
void epd_hw_init(void);

// Hardware reset + vendor sample's power-on/reset sequence.
void EPD_Reset(void);

// Vendor sample's panel init register sequence.
void EPD_init(void);

// Writes a full-screen 1bpp image buffer (EPD_BUFFER_SIZE bytes, MSB-first,
// row-byte-aligned, 0xFF = white) and triggers a full refresh.
void EPD_DisplayFull(const uint8_t *image);
