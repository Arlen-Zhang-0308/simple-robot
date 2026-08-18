#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool nrf24l01_init(void);
void nrf24l01_irq_handler(void);
void nrf24l01_process_rx(void);
bool nrf24l01_send(const uint8_t *data, size_t length);

#endif
