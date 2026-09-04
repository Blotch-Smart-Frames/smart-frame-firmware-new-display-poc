#include "epd.h"
#include "esp_log.h"
#include "images.h"

static const char *TAG = "main";

void app_main(void) {
  epd_hw_init();
  EPD_Reset();
  EPD_init();

  ESP_LOGI(TAG, "Displaying image A (charging)");
  EPD_DisplayFull(epd_bitmap_charging);

  ESP_LOGI(TAG,
           "Immediately displaying image B (splash screen) -- ghosting stress test");
  EPD_DisplayFull(epd_bitmap_splash_screen);

  ESP_LOGI(TAG, "Putting panel to sleep");
  EPD_Sleep();

  ESP_LOGI(TAG, "Done -- inspect panel for retained ghosting from image A");
}
