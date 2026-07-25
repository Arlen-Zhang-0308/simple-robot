#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transport.h"

void app_tasks_init(void);
void comm_task(void *argument);
void motion_task(void *argument);
void display_task(void *argument);
void sensor_task(void *argument);

bool comm_task_rx_byte(uint8_t byte);
size_t comm_task_process(uint8_t *tx_frame, size_t tx_capacity);
bool comm_task_transport_rx_byte(TransportId transport, uint8_t byte);
size_t comm_task_process_transport(
    TransportId *response_transport,
    uint8_t *tx_frame,
    size_t tx_capacity);
void motion_task_step(void);
void display_task_step(void);
void sensor_task_step(void);

#endif
