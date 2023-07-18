#pragma once

#include <esp_http_server.h>

httpd_handle_t start_http_server();

void stop_http_server(httpd_handle_t server);