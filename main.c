/*
 * IRIGO_Z8_BLE.c
 *
 * Created: 07.02.2018 15:16:23
 * Author : USER
 */ 

//#define F_CPU 8000000 UL

#include <avr/io.h>
#include <stdbool.h>
#include "irigo.h"
#include "utils/si_data.h"
#include "utils/UARTSI.h"
#include "utils/si_debug.h"
#include "hardware/pcf8583.h"
#include "nrf/nrf.h"
#include "utils/SPIsi.h"
#include "avr/pgmspace.h"

rtc_time rt;

void create_test_data();

		
		// 2. loading time and configuration from EEPROM


int main(void) {
	
	
	// 1. ble setup
	nrf_pins_setup();
	
	// 2. UART start
	UARTInitRXTX_conf(103, 1);                            // UBRRnL = 103 and double speed - settings for 9600 @ 8Mhz
	
	// 3. Enabling interrupts
	sei();
	dp_ln("Starting");
	
	
	// 4. Initialize SPI
    SPI_set_rate(true, SPI_SI_RATE_DIV16);  // set SPI rate as F_CPU / 8 => 1 MHz @ 8MHz F_CPU
    SPI_set_data_order(SPI_SI_LSB);
    SPI_master_init(SPI_SI_MODE0);
	
	// 5. nrf8001 module pipes map and start
//     nrf_tx_map_pipes();
//     nrf_start();		
	irigo_z8.mode = 1;

/*	
	rtc_get_time(&rt);
	rtc_print_time(&rt);
	rt.year8_t = 18;
	rtc_print_time(&rt);	
	rtc_refresh_dow(&rt);
	rtc_print_time(&rt);
*/		
	si_set_month_days(rt.year8_t);
	reset_cur_sp(&rt);
	
	//6. init irigo
// 	irigo_init_z8();
 	
	
	//make test data

	if(is_any_active_ws()) {
		dp_ln("active ws!");
	} else {
		dp_ln("no active ws");
	}

	create_test_data();
	
	if(is_any_active_ws()) {
		dp_ln("active ws!");
	} else {
		dp_ln("no active ws");
	}
	
	tws_rebuild();
	
	dp_ln("welcome");

	UARTPrintValDec_P(PSTR("sizeof(twse)="), sizeof(true_water_schedule_element));
	UARTPrintln("");
	UARTPrintValDec_P(PSTR("sizeof(tws_ea)="), sizeof(&twsea));
	UARTPrintln("");	
 	
	/* Replace with your application code */
    while (1) {
		
		// 1. aci loop
		aci_loop();
		
		
		
    }
}

void create_test_data() {
	
	irigo_z8.zone[6].water_slot[1].is_on = true;
	irigo_z8.zone[6].water_slot[1].start_hour = 7;
	irigo_z8.zone[6].water_slot[1].start_min = 15;
	irigo_z8.zone[6].water_slot[1].duration = 55;
	irigo_z8.zone[6].water_slot[1].schedule_type = ST_EVERY_DAY;
	
	irigo_z8.zone[5].water_slot[2].is_on = true;
	irigo_z8.zone[5].water_slot[2].start_hour = 23;
	irigo_z8.zone[5].water_slot[2].start_min = 35;
	irigo_z8.zone[5].water_slot[2].duration = 50;
	irigo_z8.zone[5].water_slot[2].schedule_type = ST_EVERY_DAY;	
	
	irigo_z8.zone[4].water_slot[0].is_on = true;
	irigo_z8.zone[4].water_slot[0].start_hour = 7;
	irigo_z8.zone[4].water_slot[0].start_min = 45;
	irigo_z8.zone[4].water_slot[0].duration = 65;
	irigo_z8.zone[4].water_slot[0].schedule_type = ST_DAYS_OF_WEEK;
	irigo_z8.zone[4].water_slot[0].DoW_bitmap = DOW_FRIDAY | DOW_THURSDAY;
		
}
