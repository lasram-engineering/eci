#pragma once

#define IO_INPUT0 19
#define IO_INPUT1 18
#define IO_INPUT2 5
#define IO_INPUT3 17

#define IO_INPUT_NUMBER 4

static const int map_input_to_port[IO_INPUT_NUMBER] = {IO_INPUT0, IO_INPUT1, IO_INPUT2, IO_INPUT3};

int get_input_from_port(int port);
int get_port_from_input(int input);

void init_io();