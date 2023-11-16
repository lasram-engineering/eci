#include "kawasaki.h"

#include <string.h>

#define KAWASAKI_INTERNAL_PAYLOAD_BUFFER_SIZE 32

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
esp_err_t kawasaki_read_transmission_preallocated(uart_port_t port, char *buffer, const int buffer_length, TickType_t ticks_to_wait)
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

typedef enum
{
    WAIT_ENQ,
    SEND_ACK,
    WAIT_STX,
    RECEIVE_TEXT,
    WAIT_EOT,
} kawasaki_transmission_state_machine_t;

/// @brief Reads an incoming UART transmission from a Kawasaki Controller
/// @param port the UART port to read from
/// @param payload char pointer (must be NULL). The user is expected to free the resource
/// @param ticks_to_wait number of ticks to wait before returning with timeout
/// @return esp_err_t ESP_OK if the transmission was read. ESP_ERR_TIMEOUT if a timeout has occurred
/// ESP_ERR_INVALID_ARG if the payload was not set to NULL. ESP_ERR_INVALID_RESPONSE if there was
/// no answer after the ACK message. ESP_FAIL if there was an unexpected error.
esp_err_t kawasaki_read_transmission(uart_port_t port, char **payload, TickType_t ticks_to_wait)
{
    if (payload != NULL)
        return ESP_ERR_INVALID_ARG;

    char input;
    int ret;
    kawasaki_transmission_state_machine_t state = WAIT_ENQ;

    uint16_t payload_buffer_size = KAWASAKI_INTERNAL_PAYLOAD_BUFFER_SIZE;
    uint16_t payload_index = 0;

    while (1)
    {
        // reset the input char
        input = 0;

        switch (state)
        {
        case WAIT_ENQ:
            // read input
            ret = uart_read_bytes(port, &input, 1, ticks_to_wait);

            // check for timeout
            if (ret == 0)
                return ESP_ERR_TIMEOUT;

            // if ENQ proceed to next state
            if (input == UNICODE_ENQ)
                state = SEND_ACK;

            break;

        case SEND_ACK:
            // send the ACK
            uart_write_bytes(port, &UNICODE_ACK, 1);

            // check if the payload has been acquired
            if (*payload == NULL)
                state = WAIT_STX;
            break;

            state = WAIT_EOT;
            break;

        case WAIT_STX:
            // read input
            ret = uart_read_bytes(port, &input, 1, pdTICKS_TO_MS(PROTOCOL_T3));

            if (ret == 0)
                // timeout -> 5. No answer after sending ACK
                return ESP_ERR_INVALID_RESPONSE;

            if (input == UNICODE_STX)
            {
                // allocate the text buffer
                *payload = (char *)calloc(payload_buffer_size, sizeof(char));
                // go to next state
                state = RECEIVE_TEXT;
            }
            break;

        case RECEIVE_TEXT:
            // read input
            ret = uart_read_bytes(port, &input, 1, pdTICKS_TO_MS(PROTOCOL_T3));

            if (ret == 0)
            {
                free(*payload);
                return ESP_FAIL;
            }

            // check if we have received an ETX
            if (input == UNICODE_ETX)
            {
                // trim the payload buffer if needed
                if (strlen(*payload) + 1 != payload_buffer_size)
                    *payload = (char *)realloc(*payload, sizeof(*payload) + 1);

                // go to next state
                state = SEND_ACK;
            }

            // check if the buffer is full
            if (payload_index + 1 == payload_buffer_size)
            {
                // increase the buffer size
                payload_buffer_size += KAWASAKI_INTERNAL_PAYLOAD_BUFFER_SIZE;
                // reallocate the buffer
                *payload = (char *)realloc(*payload, payload_buffer_size);
            }

            // append the input to the payload
            *payload[payload_index] = input;
            // increment the index
            payload_index++;

            break;

        case WAIT_EOT:
            // read input
            ret = uart_read_bytes(port, &input, 1, pdTICKS_TO_MS(PROTOCOL_T3));

            if (ret == 0)
            {
                free(*payload);
                return ESP_FAIL;
            }

            if (input == UNICODE_EOT)
                return ESP_OK;
        }
    }
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

#define KAWASAKI_TRANSMISSION_TYPE_POSTFIX ":"
#define KAWASAKI_TRANSMISSION_ID_POSTFIX "@"
#define KAWASAKI_TRANSMISSION_ID_CHAR '#'

#define KAWASAKI_TRANSMISSION_TYPE_MEASUREMENT "MEASUREMENT"
#define KAWASAKI_TRANSMISSION_TYPE_COMMAND "COMMAND"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief Parses a transmission payload from the Kawasaki Controller
 *
 * @param raw pointer to the raw transmission payload char array
 * @param message pointer to the empty message struct
 * @return esp_err_t
 */
esp_err_t kawasaki_parse_transmission(const char *raw, itc_message_t **message)
{
    /**
     * transmission payload syntax:
     * #<transmission_type>:<id>@<param1>|<param2>
     *
     * transmission_type can be:
     *  - COMMAND
     *  - MEASUREMENT
     */
    if (raw == NULL || !task_intercom_message_is_empty(*message))
        return ESP_ERR_INVALID_ARG;

    // copy the payload string
    char *transmission = (char *)malloc(sizeof(char) * (strlen(raw) + 1));
    strcpy(transmission, raw);

    // separate the header and payload
    char *header = strtok(transmission, KAWASAKI_TRANSMISSION_ID_POSTFIX);
    char *payload = strtok(NULL, "");

    // check if the header starts with the '#' symbol
    if (header[0] != KAWASAKI_TRANSMISSION_ID_CHAR)
    {
        free(transmission);
        return ESP_FAIL;
    }

    // get the command type
    // this is the string from the second character of the token
    char *transmission_type = strtok(header, KAWASAKI_TRANSMISSION_TYPE_POSTFIX) + 1;
    char *id_string = strtok(NULL, "");

    // convert the id to integer starting from the second character
    uint16_t id = atoi(id_string + 1);

    if (id == 0)
    {
        free(transmission);
        return ESP_FAIL;
    }

    // allocate the payload of the message
    char *payload_allocated = (char *)malloc(sizeof(char) * (strlen(payload) + 1));
    // copy the payload from the token
    strcpy(payload_allocated, payload);

    (*message)->payload = payload;
    (*message)->message_id = id;

    // check the transmission type
    if (strncmp(transmission_type, KAWASAKI_TRANSMISSION_TYPE_COMMAND, MIN(strlen(KAWASAKI_TRANSMISSION_TYPE_COMMAND), strlen(transmission_type))) == 0)
    {
        (*message)->is_measurement = false;
        free(transmission);
        return ESP_OK;
    }
    if (strncmp(transmission_type, KAWASAKI_TRANSMISSION_TYPE_MEASUREMENT, MIN(strlen(KAWASAKI_TRANSMISSION_TYPE_MEASUREMENT), strlen(transmission_type))) == 0)
    {
        (*message)->is_measurement = true;
        free(transmission);
        return ESP_OK;
    }

    free(transmission);
    return ESP_FAIL;
}

/**
 * @brief Makes and sends the response from the message object
 *
 * @param port the UART port to the Controller
 * @param message the pointer to the message object
 * @return esp_err_t ESP_ERR_INVALID_ARG if the message response is NULL. See kawasaki_write_transmission for more
 */
esp_err_t kawasaki_make_response(uart_port_t port, itc_message_t *message)
{
    char *payload;
    int ret;

    if (message->response == NULL && message->response_static == NULL)
        return ESP_ERR_INVALID_ARG;

    // allocate and print the payload
    if (message->response != NULL)
        ret = asprintf(&payload, "#%d@%s", message->message_id, message->response);

    else
        ret = asprintf(&payload, "#%d@%s", message->message_id, message->response_static);

    if (ret == -1)
        return ESP_ERR_NO_MEM;

    // send the transmission to the robot
    ret = kawasaki_write_transmission(port, payload);

    free(payload);

    return ret;
}
