#include "notification.h"
#include "volume.h"

#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdint.h>

static TaskHandle_t worker;
static SemaphoreHandle_t stopped;
static atomic_bool running;

static void play_alarm(void *arg) {
  (void)arg;
  int16_t pcm[320];
  for (unsigned i = 0; i < 320; i++)
    pcm[i] = ((i % 18) < 9) ? 5000 : -5000;
  esp_err_t rc = bsp_audio_init();
  if (rc == ESP_OK) rc = bsp_audio_wake();
  if (rc == ESP_OK) rc = bsp_audio_set_format(16000, 16, 1);
  unsigned applied_volume=101;
  while (rc == ESP_OK && atomic_load(&running)) {
    unsigned volume=ps_audio_volume(true);
    if (volume!=applied_volume) { bsp_audio_set_volume(volume); applied_volume=volume; }
    bsp_audio_write(pcm, sizeof(pcm));
    vTaskDelay(pdMS_TO_TICKS(450));
  }
  bsp_audio_sleep();
  worker = NULL;
  xSemaphoreGive(stopped);
  vTaskDelete(NULL);
}

bool ps_notification_start(void) {
  if (worker) return true;
  if (!stopped) stopped = xSemaphoreCreateBinary();
  if (!stopped) return false;
  while (xSemaphoreTake(stopped, 0) == pdTRUE) {}
  atomic_store(&running, true);
  if (xTaskCreate(play_alarm, "passport_alarm", 3072, NULL, 3, &worker) != pdPASS) {
    worker = NULL;
    atomic_store(&running, false);
    return false;
  }
  return true;
}

void ps_notification_stop(void) {
  TaskHandle_t task = worker;
  if (!task) return;
  atomic_store(&running, false);
  if (xSemaphoreTake(stopped, pdMS_TO_TICKS(2500)) == pdTRUE) return;
  /* The BSP write has a finite DMA timeout; leave the task to finish safely. */
}
