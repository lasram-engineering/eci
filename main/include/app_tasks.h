#pragma once

void error_task(void *args);

void input_task(void *args);

void internal_task(void *args);

/// @brief Measures the analog input value of the analog input pin
/// @param args
void analog_measure_task(void *args);