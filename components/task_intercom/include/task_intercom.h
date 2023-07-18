#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define SEND_BUF_LEN 32
#define RECV_BUF_LEN 32
#define ERROR_BUF_LEN 32

void get_recv(char *buffer);
void set_recv(const char *value);
bool is_empty_recv();

void get_send(char *buffer);
void set_send(const char *value);
bool is_empty_send();

void get_error(char *buffer);
void set_error(const char *value);
bool is_empty_error();