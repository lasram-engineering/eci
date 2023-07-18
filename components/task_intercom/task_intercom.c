#include "task_intercom.h"

#include <string.h>

#include <esp_log.h>
#include <esp_check.h>

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

/** Send buffer, use the get and set methods to access */
char task_intercom_send[SEND_BUF_LEN] = {'\0'};

/** Receive buffer, use the get and set methods to access */
char task_intercom_recv[RECV_BUF_LEN] = {'\0'};

/** Error buffer, use the get and set methods to access */
char task_intercom_error[ERROR_BUF_LEN] = {'\0'};

/**
 * @brief Loads the recv string into the buffer
 * @warning The buffer must be at least RECV_BUF_LEN long or a memory overwrite may occur
 *
 * @param buffer the buffer to load the value into
 */
void get_recv(char *buffer)
{
    taskENTER_CRITICAL(&spinlock);
    strcpy(buffer, task_intercom_recv);
    taskEXIT_CRITICAL(&spinlock);
}

/**
 * @brief Sets the value of the recv string
 * @attention the value must have a length of RECV_BUF_LEN - 1 or less or the program aborts
 *
 * @param value the value to be loaded into recv
 */
void set_recv(const char *value)
{
    // ESP_OK = 0 <=> false = 0
    ESP_ERROR_CHECK(!(strlen(value) <= RECV_BUF_LEN));

    taskENTER_CRITICAL(&spinlock);
    strcpy(task_intercom_recv, value);
    taskEXIT_CRITICAL(&spinlock);
}

bool is_empty_recv()
{
    bool is_empty = false;

    taskENTER_CRITICAL(&spinlock);
    is_empty = strlen(task_intercom_recv) == 0;
    taskEXIT_CRITICAL(&spinlock);

    return is_empty;
}

/**
 * @brief Loads the send string into the buffer
 * @warning The buffer must be at least SEND_BUF_LEN long or a memory overwrite may occur
 *
 * @param buffer the buffer to load the value into
 */
void get_send(char *buffer)
{
    taskENTER_CRITICAL(&spinlock);
    strcpy(buffer, task_intercom_send);
    taskEXIT_CRITICAL(&spinlock);
}

/**
 * @brief Sets the value of the send string
 * @attention the value must have a length of SEND_BUF_LEN - 1 or less or the program aborts
 *
 * @param value the value to be loaded into send
 */
void set_send(const char *value)
{
    // ESP_OK = 0 <=> false = 0
    ESP_ERROR_CHECK(!(strlen(value) <= SEND_BUF_LEN));

    taskENTER_CRITICAL(&spinlock);
    strcpy(task_intercom_send, value);
    taskEXIT_CRITICAL(&spinlock);
}

bool is_empty_send()
{
    bool is_empty = false;

    taskENTER_CRITICAL(&spinlock);
    is_empty = strlen(task_intercom_send) == 0;
    taskEXIT_CRITICAL(&spinlock);

    return is_empty;
}

/**
 * @brief Loads the error string into the buffer
 * @warning The buffer must be at least error_BUF_LEN long or a memory overwrite may occur
 *
 * @param buffer the buffer to load the value into
 */
void get_error(char *buffer)
{
    taskENTER_CRITICAL(&spinlock);
    strcpy(buffer, task_intercom_error);
    taskEXIT_CRITICAL(&spinlock);
}

/**
 * @brief Sets the value of the error string
 * @attention the value must have a length of ERROR_BUF_LEN - 1 or less or the program aborts
 *
 * @param value the value to be loaded into error
 */
void set_error(const char *value)
{
    // ESP_OK = 0 <=> false = 0
    ESP_ERROR_CHECK(!(strlen(value) <= ERROR_BUF_LEN));

    taskENTER_CRITICAL(&spinlock);
    strcpy(task_intercom_error, value);
    taskEXIT_CRITICAL(&spinlock);
}

bool is_empty_error()
{
    bool is_empty = false;

    taskENTER_CRITICAL(&spinlock);
    is_empty = strlen(task_intercom_error) == 0;
    taskEXIT_CRITICAL(&spinlock);

    return is_empty;
}