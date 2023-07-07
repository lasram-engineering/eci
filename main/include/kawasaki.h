#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/uart.h>
#include <esp_log.h>

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

esp_err_t kawasaki_read_transmission(uart_port_t port, char *buffer, const int buffer_length, SemaphoreHandle_t lock);

esp_err_t kawasaki_write_transmission(uart_port_t port, const char *payload, SemaphoreHandle_t lock);