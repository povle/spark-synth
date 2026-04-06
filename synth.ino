#include "src/System.h"

TaskHandle_t inputTaskHandle = NULL;

void input_task_wrapper(void* pvParameters) {
    while(1) {
        if (System.useBackgroundTask) {
            // SYNTH MODE: Scan as fast as possible on Core 0
            System.inputTask();
            vTaskDelay(pdMS_TO_TICKS(2));
        } else {
            // BLE MODE: Do absolutely nothing. Sleep cleanly. 
            // Core 0 is now 100% free for the Bluetooth Radio.
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void setup() {
    System.begin();

    xTaskCreatePinnedToCore(
        input_task_wrapper, "InputTask", 4096, NULL, 2, &inputTaskHandle, 0
    );
}

void loop() {

    if (System.isBleActive()) {
        // --- BLE MODE ---
        if (System.useBackgroundTask) {
            // 1. Tell the background task to stop
            System.useBackgroundTask = false;
            // 2. Wait 5ms to guarantee it has finished its current I2C read
            delay(5); 
        }
        
        // 3. Scan the keyboard here on Core 1 (Safe from the Radio)
        System.inputTask();
        
    } else {
        // --- SYNTH MODE ---
        // Let the high-speed background task handle it
        System.useBackgroundTask = true;
    }
    
    // Pacing delay
    vTaskDelay(pdMS_TO_TICKS(5));

    System.update();
}