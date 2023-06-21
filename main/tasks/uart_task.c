#include "tasks/uart_task.h"

#include <string.h>
#include <stdio.h>

#include <driver/uart.h>
#include <esp_log.h>

#include "app_state.h"

// UART pins for MAU
#define UART_RX_MAU 2
#define UART_TX_MAU 0
#define UART_RTS_MAU 4
#define UART_CTS_MAU 16
#define UART_BAUD_MAU 115200

// UART pins for robot
#define UART_RX 17
#define UART_TX 5
#define UART_RTS 18
#define UART_CTS 19
#define UART_BAUD_ROBOT 115200

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

#define UART_MAX_RESPONSE_LENGTH 32

/** If no payload arrives after this time has elapsed from the ACK send end the transmission */
#define PROTOCOL_T3 1000

// transmission flags from the Kawasaki controller
/** Enquiry, this means the beginning of a transmission */
static const char UNICODE_ENQ = 5;
/** Acknowledge */
static const char UNICODE_ACK = 6;
/** Start of text, after this comes the payload */
static const char UNICODE_STX = 2;
/** End of text, this follows the payload */
static const char UNICODE_ETX = 3;
/** End of transmission, this closes the transmission */
static const char UNICODE_EOT = 4;

static char *TAG = "UART";
static char uart_response[UART_MAX_RESPONSE_LENGTH];

/**
 * Configuration for UART MAU
 */
static uart_config_t uart_config_mau = {
    .baud_rate = UART_BAUD_MAU,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

/**
 * Configuration for UART robot
 */
static uart_config_t uart_config_robot = {
    .baud_rate = UART_BAUD_ROBOT,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // TODO test this
    .source_clk = UART_SCLK_DEFAULT,
};

const uart_port_t uart_mau = UART_NUM_2;
const uart_port_t uart_robot = UART_NUM_1;

QueueHandle_t uart_queue_mau;
QueueHandle_t uart_queue_robot;

/**
 * Reads from the UART device until a newline termination character is found
 * The found string is stored in the uart_response char array
 *
 * @param port: the UART port to read from
 */
esp_err_t uart_read_string(uart_port_t port)
{
    int i, ret;
    for (i = 0; i < UART_MAX_RESPONSE_LENGTH; i++)
    {
        // block until a response is sent
        ret = uart_read_bytes(port, &uart_response[i], 1, 3000 / portTICK_PERIOD_MS);

        if (ret <= 0)
        {
            // the read has timed out
            return ESP_ERR_TIMEOUT;
        }

        if (uart_response[i] == '\n')
            break;
    }

    // replace the terminating newline with a zero
    uart_response[i] = '\0';

    return ESP_OK;
}

/**
 * Waits until an incoming transmission from the robot controller
 * When the transmission comes in, decodes it and puts it in the response string
 */
esp_err_t uart_read_transmission(uart_port_t port)
{
    char input;
    int ret;

    while (input != UNICODE_ENQ)
    {
        // wait until an ENQ byte comes
        // hangs indefinitely
        ret = uart_read_bytes(port, &input, 1, portMAX_DELAY);

        if (ret <= 0)
            return ESP_ERR_TIMEOUT;
    }

    // after the ENQ is received, send an ACK
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // after the ACK was sent, the controller will send a STX
    ret = uart_read_bytes(port, &input, 1, PROTOCOL_T3 / portTICK_PERIOD_MS);

    if (ret <= 0 || input != UNICODE_STX)
    {
        // send abnormal EOT back
        uart_write_bytes(port, &UNICODE_EOT, 1);
        return ESP_ERR_INVALID_RESPONSE;
    }
    // after this, read until the ETX flag is sent
    int i = 0;
    for (i = 0; i < UART_MAX_RESPONSE_LENGTH; i++)
    {
        // read byte by byte
        ret = uart_read_bytes(port, &input, 1, portMAX_DELAY);

        if (ret <= 0)
            return ESP_ERR_TIMEOUT;

        if (input != UNICODE_ETX)
        {
            uart_response[i] = input;
        }
        else
        {
            // the payload has ended
            // terminate the string
            uart_response[i] = '\0';

            break;
        }
    }

    // send back the ACK signal
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // receive the EOT flag
    uart_read_bytes(port, &input, 1, portMAX_DELAY);

    return ESP_OK;
}

void uart_task(void *arg)
{
    ESP_LOGI(TAG, "Initializing UART...");

    // setup the UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_mau, &uart_config_mau));
    ESP_ERROR_CHECK(uart_param_config(uart_robot, &uart_config_robot));

    // set the pins to the uart
    ESP_ERROR_CHECK(uart_set_pin(uart_mau, UART_TX_MAU, UART_RX_MAU, UART_RTS, UART_CTS));
    ESP_ERROR_CHECK(uart_set_pin(uart_robot, UART_TX, UART_RX, UART_RTS, UART_CTS));

    // install driver
    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_mau,
            UART_RX_BUFFER_SIZE,
            UART_TX_BUFFER_SIZE,
            UART_QUEUE_SIZE,
            &uart_queue_mau, 0));

    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_robot,
            UART_RX_BUFFER_SIZE,
            UART_TX_BUFFER_SIZE,
            UART_QUEUE_SIZE,
            &uart_queue_robot, 0));

    ESP_LOGI(TAG, "Initialization done.");

    bool motorRun = false;
    char *startMotor = "DO|MOTOR ON\n";
    char *stopMotor = "DO|MOTOR OFF\n";
    /**
     * Can be used to store motor speed commands with 4 digits
     */
    char setMotorSpeed[21];

    esp_err_t ret;

    while (1)
    {
        // wait for the transmission
        ret = uart_read_transmission(uart_robot);

        if (ret == ESP_ERR_TIMEOUT)
            ESP_LOGE(TAG, "Read timed out");
        else if (ret == ESP_ERR_INVALID_RESPONSE)
            ESP_LOGE(TAG, "Invalid response");
        else
            ESP_LOGI(TAG, "Incoming transmission: %s", uart_response);
    }
}