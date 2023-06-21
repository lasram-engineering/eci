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

/**
 * If both the robot and the ECI initiaites a transmission, the robot won't ACK the transmission
 * This is an ENQ collision
 *
 * Basically how long we have to wait for an ACK signal
 */
#define PROTOCOL_T1 100
/** The time to wait for an ACK after sending the payload */
#define PROTOCOL_T2 100
/** The time to wait for the payload after the ACK signal */
#define PROTOCOL_T3 1000
/** The number of retries after not receiving ACK in T2 time*/
#define PROTOCOL_C1 3
/** The number of retries after a NAK response */
#define PROTOCOL_C2 3

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
/** Not acknowledge */
static const char UNICODE_NAK = 15;

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
 * Waits for the STX and then receives the payload and waits for the ETX flag
 * @returns
 *  ESP_OK on success,
 *  ESP_FAIL if there was an error,
 *  ESP_ERR_INVALID_RESPONSE if the response was invalid,
 *  ESP_ERR_TIMEOUT if there was a timeout
 */
esp_err_t uart_read_transmission_payload(uart_port_t port)
{
    esp_err_t ret;
    char input;

    while (1)
    {
        // wait for the ACK (or EOT)
        ret = uart_read_bytes(port, &input, 1, PROTOCOL_T3 / portTICK_PERIOD_MS);

        // check for timeout
        if (ret == 0)
        {
            // send abnormal EOT back
            uart_write_bytes(port, &UNICODE_EOT, 1);
            return ESP_ERR_TIMEOUT;
        }

        // abnormal EOT (likely due to too many retries)
        if (input == UNICODE_EOT)
            return ESP_FAIL;

        // if the token is not STX it is an invalid response
        else if (input != UNICODE_STX)
            return ESP_ERR_INVALID_RESPONSE;

        // normal operation, receive the payload
        int c;
        for (c = 0; c < UART_MAX_RESPONSE_LENGTH; c++)
        {
            // read byte by byte
            ret = uart_read_bytes(port, &input, 1, PROTOCOL_T3 / portTICK_PERIOD_MS);

            // check for timeout
            if (ret == 0)
            {
                // terminate abnormally
                uart_write_bytes(port, &UNICODE_EOT, 1);
                return ESP_ERR_TIMEOUT;
            }

            // check if the text is ended
            if (input == UNICODE_ETX)
            {
                // terminate the string
                uart_response[c] = '\0';
                return ESP_OK;
            }
            else
            {
                // append the input to the string
                uart_response[c] = input;
            }
        }

        // if program reaches here it means that ETX was never received
        // and the max uart response length was reached
        // send a NACK to the controller
        uart_write_bytes(port, &UNICODE_NAK, 1);
        ESP_LOGW(TAG, "Response too long, sending back NACK and retrying...");
    }
}

/**
 * Waits until an incoming transmission from the robot controller
 * When the transmission comes in, decodes it and puts it in the response string
 * @returns
 *  ESP_OK if successful,
 *  ESP_FAIL if there was an error,
 *  ESP_TIMEOUT if there was a timeout
 *  ESP_ERR_INVALID_RESPONSE if the response was invalid
 */
esp_err_t uart_read_transmission(uart_port_t port)
{
    char input;
    esp_err_t ret;

    while (input != UNICODE_ENQ)
    {
        // wait until an ENQ byte comes
        // hangs indefinitely
        ret = uart_read_bytes(port, &input, 1, portMAX_DELAY);

        if (ret == 0)
            return ESP_ERR_TIMEOUT;
    }

    // after the ENQ is received, send an ACK
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // read the transmission payload
    ret = uart_read_transmission_payload(port);

    if (ret != ESP_OK)
        return ret;

    // send back the ACK signal
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // receive the EOT flag
    uart_read_bytes(port, &input, 1, portMAX_DELAY);

    return ESP_OK;
}

/**
 * Sends a transmission with the given payload to the robot controller
 * @returns
 *  ESP_OK if success,
 *  ESP_INVALID_RESPONSE if a response was invalid
 *  ESP_ERR_NOT_FINISHED if there was an ENQ collision (check uart_response)
 *  ESP_FAIL if the transmission has failed
 */
esp_err_t uart_write_transmission(uart_port_t port, const char *payload)
{
    esp_err_t ret;
    char input;
    int retry;
    bool skip_enq = false;

    // No answer means retry for at max C1 times
    for (retry = 1; retry <= PROTOCOL_C1; retry++)
    {
        if (!skip_enq)
        {
            // initiate the transmission with the ENQ token
            uart_write_bytes(port, &UNICODE_ENQ, 1);

            // wait for the ACK token
            ret = uart_read_bytes(port, &input, 1, PROTOCOL_T1 / portTICK_PERIOD_MS);

            // ENQ collision
            // this can happen if there is a timeout or the input is an ENQ
            if (ret == 0 || input == UNICODE_ENQ)
            {
                return ESP_ERR_NOT_FINISHED;
            }
        }

        // normal operation
        // send the STX token
        uart_write_bytes(port, &UNICODE_STX, 1);
        // send the payload
        uart_write_bytes(port, payload, strlen(payload));
        // send the ETX token
        uart_write_bytes(port, &UNICODE_ETX, 1);

        // wait for the ACK/NACK for T2
        ret = uart_read_bytes(port, &input, 1, PROTOCOL_T2 / portTICK_PERIOD_MS);

        // if no response, retry
        if (ret <= 0)
            continue;

        // response is either ACK or NACK
        if (input == UNICODE_NAK)
        {
            // retry sending the payload
            skip_enq = true;
            continue;
        }
        else if (input == UNICODE_ACK)
        {
            // terminate normally
            uart_write_bytes(port, &UNICODE_EOT, 1);
            return ESP_OK;
        }
        else
        {
            // response is invalid
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    // if we reach this point there were too many retries and no answer
    // terminate abnormally
    uart_write_bytes(port, &UNICODE_EOT, 1);

    return ESP_FAIL;
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

        switch (ret)
        {
        case ESP_FAIL:
            ESP_LOGW(TAG, "Incoming transmission read failed");
            break;

        case ESP_ERR_TIMEOUT:
            ESP_LOGW(TAG, "Incoming transmission has timed out");
            break;

        case ESP_ERR_INVALID_RESPONSE:
            ESP_LOGW(TAG, "Incoming transmission had invalid response");
            break;

        case ESP_OK:
            ESP_LOGI(TAG, "Incoming transmission %s", uart_response);

        default:
            break;
        }
    }
}