#include "uart.h"

/**
 * @brief Reads a line from the UART terminal. The transmission ends when a newline character is read
 *
 * @param port the uart port to read from
 * @param buffer the buffer to read into
 * @param buffer_length the length of the buffer
 * @param delay_ticks the maximum delay in ticks to wait for the incoming data
 * @return esp_err_t    ESP_OK if the transmission was successful,
 *                      ESP_FAIL if there was an error,
 *                      ESP_TIMEOUT if there was a timeout,
 *                      ESP_ERR_NOT_FINISHED if the buffer was not long enough for the string
 */
esp_err_t uart_read_string(uart_port_t port, char *buffer, const unsigned int buffer_length, TickType_t delay_ticks)
{
    int i, ret;
    char input;
    for (i = 0; i < buffer_length; i++)
    {
        // block unitl a byte is incomingm, or the timeout happens
        ret = uart_read_bytes(port, &input, 1, delay_ticks);

        if (ret == 0)
            return ESP_ERR_TIMEOUT;

        if (ret < 0)
            return ESP_FAIL;

        if (input == '\n')
        {
            // terminate the string
            buffer[i] = '\0';
            return ESP_OK;
        }

        // write the input character into the buffer
        buffer[i] = input;
    }

    // if we reach here then the incoming string was longer than the buffer
    return ESP_ERR_NOT_FINISHED;
}