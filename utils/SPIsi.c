/*
 * SPIsi.c
 *
 * Created: 25.10.2016 15:09:20
 *  Author: USER
 */ 
#include <avr/io.h>
#include "SPIsi.h"
#include <avr/interrupt.h>
//#include <util/delay.h>
#include <avr/sfr_defs.h>

//--------------------------------------------------------------------------------------------------------------------------------
void SPI_set_data_order(uint8_t data_order) {
	if (data_order == SPI_SI_MSB)	{
		SPCR &= ~_BV(DORD);		
	} else {
		SPCR |= _BV(DORD);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------
/*	        SPI2X  SPR1  SPR0       SCK frequency
	        0     0     0           F_CPU/4
	        0     0     1           F_CPU/16
	        0     1     0           F_CPU/64
	        0     1     1           F_CPU/128
	        1     0     0           F_CPU/2
	        1     0     1           F_CPU/8
	        1     1     0           F_CPU/32
	        1     1     1           F_CPU/64 */
void SPI_set_rate(bool is_double_rate, uint8_t rate) {

	SPCR &= ~(_BV(SPI2X)| _BV(SPR1) | _BV(SPR0)); // clear rate bits
	
	if(is_double_rate) { 
		SPCR |=_BV(SPI2X);
	}
	
	if(rate == SPI_SI_RATE_DIV16 || rate == SPI_SI_RATE_DIV128 ) {
		SPCR |= _BV(SPR0);
	} 

	if(rate == SPI_SI_RATE_DIV64 || rate == SPI_SI_RATE_DIV128 ) {
		SPCR |= _BV(SPR1);
	}

}
//--------------------------------------------------------------------------------------------------------------------------------
/*
The four modes combine polarity and phase according to this table:
Mode	  | Clock Polarity | Clock Phase  |    Output Edge	| Data Capture   |
		  |		(CPOL)     |    (CPHA)    |                 |				 |
SPI_MODE0		  0				  0			    Falling			 Rising
SPI_MODE1		  0				  1			    Rising			 Falling
SPI_MODE2		  1				  0			    Rising			 Falling
SPI_MODE3		  1				  1			    Falling			 Rising
*/
void SPI_master_init(uint8_t mode) {
	DDRB  |= _BV(PB3) | _BV(PB5) | _BV(PB2);				// set MOSI and SCK and SS (!) pins as output, leaving MISO as input. SPI doesn't work if SS is not configured as OUTPUT (!!!)	
	PORTB |= _BV(PB2);										// set SS to high
		
	SPCR |= _BV(SPE) | _BV(MSTR);// | _BV(SPIE);			    // turn spi on, set as master, MSB transfered fist (DORD bit is 0)
	
	
	if(mode == SPI_SI_MODE1 || mode == SPI_SI_MODE3 ) {				// set mode
		SPCR |= _BV(CPHA);
	}

	if(mode == SPI_SI_MODE2 || mode == SPI_SI_MODE3 ) {				// set mode
		SPCR |= _BV(CPOL);
	}
	
  /*    SPIF  - Флаг прерывания от SPI. уст в 1 по окончании передачи байта
        WCOL  - Флаг конфликта записи. Уст в 1 если байт не передан, а уже попытка записать новый.
        SPI2X - Удвоение скорости обмена.*/	
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SPI_master_transmit(uint8_t data) {
	SPDR = data;								// Start transmission
	while(bit_is_clear(SPSR, SPIF));			// Wait for transmission complete
	return SPDR;								// Return data received from SPI slave device
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SPI_get_data() {
	return SPDR;
}
//--------------------------------------------------------------------------------------------------------------------------------
// ISR(SPI_STC_vect) {
// 	++spi_transaction_complete;
// }
//--------------------------------------------------------------------------------------------------------------------------------

