#pragma once

#define IO_INPUT0 34
#define IO_INPUT1 35
#define IO_INPUT2 32
#define IO_INPUT3 33

#define IO_INPUT_NUMBER 4

#define IO_OUTPUT0 19
#define IO_OUTPUT1 18
#define IO_OUTPUT2 5
#define IO_OUTPUT3 17

#define IO_OUTPUT_NUMBER 4

static const int map_input_to_port[IO_INPUT_NUMBER] = {IO_INPUT0, IO_INPUT1, IO_INPUT2, IO_INPUT3};
static const int map_output_to_port[IO_OUTPUT_NUMBER] = {IO_OUTPUT0, IO_OUTPUT1, IO_OUTPUT2, IO_OUTPUT3};

int get_input_from_port(int port);
int get_port_from_input(int input);

int get_output_from_port(int port);
int get_port_from_output(int output);

void set_output_level(int output, int level);

void init_io();