#include "src/System.h"

TaskHandle_t inputTaskHandle = NULL;

void input_task_wrapper(void* pvParameters) {
    while(1) {
        System.inputTask();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup() {
    System.begin();

    xTaskCreatePinnedToCore(
        input_task_wrapper, "InputTask", 4096, NULL, 5, &inputTaskHandle, 0
    );
}

void loop() {
    if (System.isBleActive()) {
        // --- SLOW POLLING MODE (for BLE Stability) ---
        // For some reason BLE MIDI is laggy when the input is in a separate task.

        if (eTaskGetState(inputTaskHandle) != eSuspended) {
            vTaskSuspend(inputTaskHandle);
            Serial.println("SUSPENDED fast input task.");
        }

        System.update();
        System.inputTask(); // This polls keys/pots/joy once

    } else {
        // --- FAST POLLING MODE (for AMY Audio) ---
        // Ensure the input task is running
        if (eTaskGetState(inputTaskHandle) == eSuspended) {
            vTaskResume(inputTaskHandle);
            Serial.println("RESUMED fast input task.");
        }
        System.update();
    }

}
