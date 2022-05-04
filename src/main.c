#include <generic_esp_32.h>
#include <scheduler.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <math.h>
#include <rtc.h>

#ifdef CONFIG_TWOMES_PRESENCE_DETECTION
#define DEVICE_TYPE_NAME "Presence-Detector"
static const char *TAG = "Twomes ESP32 presence detector";
#else
#define DEVICE_TYPE_NAME "Generic-Test"
static const char *TAG = "Twomes ESP32 generic test device";
#endif

#define BOOT_STARTUP_INTERVAL_MS (10 * 1000) // milliseconds ( 10 s * 1000 ms/s)
#define BOOT_STARTUP_INTERVAL_TXT "Wating 10 seconds before next measurement data series is started"

void app_main(void)
{
#if defined ESP32DEV
    twomes_device_provisioning(DEVICE_TYPE_NAME);
    // TODO: move tasks to new twomes_device_initialization() function in generic firmware library

    ESP_LOGD(TAG, "Starting heartbeat task");
    xTaskCreatePinnedToCore(&heartbeat_task, "heartbeat_task", 4096, NULL, 1, NULL, 1);
    
    ESP_LOGD(TAG, BOOT_STARTUP_INTERVAL_TXT);
    vTaskDelay(BOOT_STARTUP_INTERVAL_MS / portTICK_PERIOD_MS);
    
    ESP_LOGD(TAG, "Starting timesync task");
    xTaskCreatePinnedToCore(&timesync_task, "timesync_task", 4096, NULL, 1, NULL, 1);
    
    ESP_LOGD(TAG, BOOT_STARTUP_INTERVAL_TXT);
    vTaskDelay(BOOT_STARTUP_INTERVAL_MS / portTICK_PERIOD_MS);

#ifdef CONFIG_TWOMES_PRESENCE_DETECTION
    ESP_LOGD(TAG, "Starting presence detection");
    start_presence_detection();
#endif

#elif defined M5STACK_COREINK
    ESP_LOGD(TAG, "Target is M5Stack_CoreINK");
#endif
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS); //
    }
}