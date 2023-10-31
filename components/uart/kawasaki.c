#include "kawasaki.h"

#include <string.h>

#include <driver/uart.h>
#include <esp_log.h>

static const char *TAG = "Kawasaki";

/**
 * Waits for the STX and then receives the payload and waits for the ETX flag
 * @returns
 *  ESP_OK on success,
 *  ESP_FAIL if there was an error,
 *  ESP_ERR_INVALID_RESPONSE if the response was invalid,
 *  ESP_ERR_TIMEOUT if there was a timeout
 */
esp_err_t kawasaki_read_transmission_payload(uart_port_t port, char *buffer, const int buffer_length)
{
    esp_err_t ret;
    char input;

    while (1)
    {
        // wait for the STX (or EOT)
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
        for (c = 0; c < buffer_length; c++)
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
                buffer[c] = '\0';
                return ESP_OK;
            }
            else
            {
                // append the input to the string
                buffer[c] = input;
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
 * @param port the uart port to use
 * @param buffer the character buffer to write the transmission into
 * @param buffer_length the length of the input buffer
 * @param ticks_to_wait the time in ticks to wait before timing out
 * @returns
 *  ESP_OK if successful,
 *  ESP_FAIL if there was an error,
 *  ESP_TIMEOUT if there was a timeout
 *  ESP_ERR_INVALID_RESPONSE if the response was invalid
 */
esp_err_t kawasaki_read_transmission(uart_port_t port, char *buffer, const int buffer_length, TickType_t ticks_to_wait)
{

    // wait until the semaphore is available
    // xSemaphoreTake(lock, portMAX_DELAY);

    char input = 0;
    esp_err_t ret;

    while (input != UNICODE_ENQ)
    {
        // wait until an ENQ byte comes
        // hangs indefinitely
        ret = uart_read_bytes(port, &input, 1, ticks_to_wait);

        // a timeout has occurred
        if (ret == 0)
        {
            return ESP_ERR_TIMEOUT;
        }

        // the connection has been closed
        if (input == UNICODE_EOT)
        {
            return ESP_OK;
        }
    }

    // after the ENQ is received, send an ACK
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // wait until the TX buffer is empty
    uart_wait_tx_done(port, PROTOCOL_T2 / portTICK_PERIOD_MS);

    // read the transmission payload
    ret = kawasaki_read_transmission_payload(port, buffer, buffer_length);

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Error while reading UART transmission: %s", esp_err_to_name(ret));
        return ret;
    }

    // send back the ACK signal
    uart_write_bytes(port, &UNICODE_ACK, 1);

    // receive the EOT flag
    uart_read_bytes(port, &input, 1, PROTOCOL_T2 / portTICK_PERIOD_MS);

    return ESP_OK;
}

/**
 * Sends a transmission with the given payload to the robot controller via the specified UART port
 * @param port the UART port to use for the transmission
 * @param payload the buffer to store the payload into
 * @param lock the semaphore object to block other transmissions from using the UART line
 * @returns
 *  ESP_OK if success,
 *  ESP_INVALID_RESPONSE if a response was invalid
 *  ESP_ERR_NOT_FINISHED if there was an ENQ collision (check uart_response)
 *  ESP_ERR_TIMEOUT if a timeout has occurred
 *  ESP_FAIL if the transmission has failed
 */
esp_err_t kawasaki_write_transmission(uart_port_t port, const char *payload)
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
            if (ret == 0)
            {
                return ESP_ERR_TIMEOUT;
            }
            if (input == UNICODE_ENQ)
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

        // if timeout (no response), retry with inquiry
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