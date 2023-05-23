#include "io.h"

#include <esp_event.h>
#include <esp_log.h>
#include <driver/gpio.h>

#include "app_state.h"

static const char *TAG = "IO";

// define the logical high and low values for gpio pins
#ifdef CONFIG_INPUT_PULL_MODE_PULLUP
#define IO_LOGICAL_HIGH 0
#define IO_LOGICAL_LOW 1
#endif
#ifdef CONFIG_INPUT_PULL_MODE_PULLDOWN
#define IO_LOGICAL_HIGH 1
#define IO_LOGICAL_LOW 0
#endif

int get_input_from_port(int port)
{
    int i;
    for (i = 0; i < IO_INPUT_NUMBER; i++)
    {
        if (map_input_to_port[i] == port)
        {
            return i;
        }
    }

    return -1;
}

int get_port_from_input(int input)
{
    if (input < IO_INPUT_NUMBER && input >= 0)
    {
        return map_input_to_port[input];
    }

    return -1;
}

int get_output_from_port(int port)
{
    int output;
    for (output = 0; output < IO_OUTPUT_NUMBER; output++)
    {
        if (map_output_to_port[output] == port)
        {
            return output;
        }
    }

    return -1;
}

int get_port_from_output(int output)
{
    if (output < IO_OUTPUT_NUMBER && output >= 0)
    {
        return map_output_to_port[output];
    }

    return -1;
}

void set_output_level(int output, int level)
{
    int output_port = get_port_from_output(output);

    gpio_set_level(output_port, level);
}

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    ESP_DRAM_LOGI(TAG, "GPIO interrupt");
    int pin_number = (int)args;

    int input_number = get_input_from_port(pin_number);

    // update the application state
    if (gpio_get_level(pin_number) == IO_LOGICAL_HIGH)
    {
        app_state_set(STATE_TYPE_INPUT, BIT(input_number + 1));
    }
    else
    {
        app_state_unset(STATE_TYPE_INPUT, BIT(input_number + 1));
    }
}

void init_io()
{
    int input, output, pin;

    // initialize isr
    gpio_install_isr_service(0);

    // set the inputs
    for (input = 0; input < IO_INPUT_NUMBER; input++)
    {
        pin = get_port_from_input(input);

        ESP_LOGI(TAG, "Initializing input %d at pin %d", input, pin);
        // set the direction of the pin
        gpio_set_direction(pin, GPIO_MODE_INPUT);

        // set the pin pull mode
#ifdef CONFIG_INPUT_PULL_MODE_PULLUP
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
#endif
#ifdef CONFIG_INPUT_PULL_MODE_PULLDOWN
        gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
#endif
        // set interrupt edge
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);

        // add the isr handler function
        gpio_isr_handler_add(pin, gpio_interrupt_handler, (void *)pin);
    }

    // set the outputs
    for (output = 0; output < IO_OUTPUT_NUMBER; output++)
    {
        pin = get_port_from_output(output);

        ESP_LOGI(TAG, "Initializing output %d at pin %d", output, pin);

        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    }
}
