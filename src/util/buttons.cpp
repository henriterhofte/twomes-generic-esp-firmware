#include <util/buttons.hpp>

#include <freertos/task.h>

#include <util/error.hpp>
#include <util/delay.hpp>

constexpr const char *TAG = "Buttons";

constexpr int LONG_BUTTON_PRESS_DURATION = 10 * 2; // Number of half seconds minus one to wait: (10 s * 2 halfseconds)

namespace Buttons
{
    namespace
    {
        // Event queue used by the ISR and handler task.
        static xQueueHandle s_gpioEventQueue;

        // Vector to keep track of all buttons.
        static std::vector<std::shared_ptr<Button>> s_buttons;

        /**
         * Interrupt service handler for button presses.
         *
         * @param pvParams Task parameters.
         */
        void IRAM_ATTR ISRHandlerGPIO(void *pvParams)
        {
            xQueueSendFromISR(s_gpioEventQueue, pvParams, NULL);
        }

        /**
         * FreeRTOS task that is used to handle button presses.
         */
        void ButtonPressHandlerTask(void *pvParams)
        {
            auto button = new Button;

            while (true)
            {
                // Wait for an event on the queue.
                xQueueReceive(s_gpioEventQueue, button, portMAX_DELAY);

                // Debounce: nothing happens when button is pressed < 0.1 s
                vTaskDelay(Delay::MilliSeconds(100));

                for (int halfSeconds = 1; gpio_get_level(button->gpioNum) == button->trigger; halfSeconds++)
                {
                    // Button is still pressed.

                    // Wait for half a second to pass.
                    vTaskDelay(Delay::MilliSeconds(500));

                    // Call the buttons callback based on the duration of the press.
                    if (halfSeconds == LONG_BUTTON_PRESS_DURATION)
                    {
                        if (button->longPressCallback == nullptr)
                        {
                            ESP_LOGD(TAG,
                                     "Long press was detected for button \"%s\", but no long press callback was defined.",
                                     button->name.c_str());
                            break;
                        }

                        ESP_LOGD(TAG,
                                 "Long press was detected for button \"%s\". Invoking callback.",
                                 button->name.c_str());
                        button->longPressCallback();
                    }
                    else if (gpio_get_level(button->gpioNum) != button->trigger)
                    {
                        // Button was released before it was considered a long press.

                        if (button->shortPressCallback == nullptr)
                        {
                            ESP_LOGD(TAG,
                                     "Short press was detected for button \"%s\", but no short press callback was defined.",
                                     button->name.c_str());
                            break;
                        }

                        ESP_LOGD(TAG,
                                 "Short press was detected for button \"%s\". Invoking callback.",
                                 button->name.c_str());
                        button->shortPressCallback();
                    }
                }
            }
        }
    } // namespace

    namespace ButtonPressHandler
    {
        namespace
        {
            static bool s_initialized = false;

            void Init()
            {
                s_gpioEventQueue = xQueueCreate(10, sizeof(Button));

                auto err = gpio_install_isr_service(0);
                Error::CheckAppendName(err, TAG, "An error occured when installing isr service");

                err = xTaskCreatePinnedToCore(ButtonPressHandlerTask,
                                              "ButtonPressHandlerTask",
                                              2048,
                                              nullptr,
                                              10,
                                              nullptr,
                                              APP_CPU_NUM);
                if (err != pdPASS)
                    ESP_LOGE(TAG, "An error occured when starting button press handler task");
            }
        } // namespace

        esp_err_t AddButton(gpio_num_t gpioNum,
                            const std::string &name,
                            int trigger,
                            ButtonCallback_t cbShort,
                            ButtonCallback_t cbLong)
        {
            if (!s_initialized)
                Init();

            std::unique_ptr<Button> button(new Button);
            button->gpioNum = gpioNum;
            button->name = name;
            button->trigger = trigger;
            button->shortPressCallback = cbShort;
            button->longPressCallback = cbLong;

            ESP_LOGD(TAG, "Button added.\n"
                          "\tGPIO num: %d\n"
                          "\tName: %s\n"
                          "\tTrigger: %d\n"
                          "\tShort press callback: %s\n"
                          "\tLong press callback: %s",
                     button->gpioNum,
                     button->name.c_str(),
                     button->trigger,
                     button->shortPressCallback ? "true" : "false",
                     button->longPressCallback ? "true" : "false");

            // Add it to m_buttons so memory can be freed if needed.
            s_buttons.push_back(std::move(button));

            auto err = gpio_isr_handler_add(gpioNum, ISRHandlerGPIO, s_buttons.back().get());
            if (Error::CheckAppendName(err, TAG, "An error occured when adding isr handler"))
                return err;

            return ESP_OK;
        }
    } // namespace ButtonPressHandler
} // namespace Buttons
