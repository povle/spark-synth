#include "src/System.h"

void input_task_wrapper(void *pvParameters)
{
    while (1)
    {
        System.inputTask();

        // Run at ~500Hz, yielding to other tasks like the main loop
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup()
{
    // Your existing setup() code is perfect. It initializes everything.
    System.begin();

    // After System.begin(), launch the high-priority input task on Core 0
    xTaskCreatePinnedToCore(
        input_task_wrapper,
        "InputTask", // Task name
        4096,        // Stack size
        NULL,        // Parameters
        5,           // Priority (high)
        NULL,        // Task handle
        0            // Core
    );
}

void loop()
{
    System.update();
}
