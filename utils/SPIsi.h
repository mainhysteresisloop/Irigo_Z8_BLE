/*
 * SPIsi.h
 *
 * Created: 25.10.2016 22:03:39
 *  Author:  Sergey Shelepin
 */ 


#ifndef SPISI_H_
#define SPISI_H_

#include <stdbool.h>

#define SPI_SI_MSB 0
#define SPI_SI_LSB 1

#define SPI_SI_RATE_DIV4 0
#define SPI_SI_RATE_DIV16 1
#define SPI_SI_RATE_DIV64 2
#define SPI_SI_RATE_DIV128 3

#define SPI_SI_MODE0 0
#define SPI_SI_MODE1 1
#define SPI_SI_MODE2 2
#define SPI_SI_MODE3 3


volatile uint8_t spi_transaction_complete;

void SPI_master_init(uint8_t mode);
void SPI_set_data_order(uint8_t data_order);
void SPI_set_rate(bool is_double_rate, uint8_t rate);
uint8_t SPI_master_transmit(uint8_t data);
uint8_t SPI_get_data();



#endif /* SPISI_H_ */