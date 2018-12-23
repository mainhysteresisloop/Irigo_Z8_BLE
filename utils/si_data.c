/*
 * si_data.c
 *
 * Created: 26.02.2018 15:18:24
 *  Author:   Sergey Shelepin
 */ 

#include "si_data.h"
#include <stdbool.h>
#include "UARTSI.h"


extern void zone_config_parser(uint8_t * cfg, uint8_t zone_num); 
extern void print_water_config();

//----------------------------------------------------------------------
//  check if year is leap year
//----------------------------------------------------------------------
uint8_t is_leap_year(uint16_t year) {

	if( (year%400 == 0 || year%100 != 0) && (year%4 == 0)) {
		return true;
	}
	return false;
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void correct_feb_days(uint8_t year8_t) {
	if (is_leap_year((uint16_t)year8_t + 2000)) {
		si_days_in_month[1] = 29;
		} else {
		si_days_in_month[1] = 28;
	}
}


void si_set_month_days(uint8_t year8_t) {
	si_days_in_month[0] = 31;    //Jan
	si_days_in_month[1] = 28;    //Feb
	si_days_in_month[2] = 31;    //Mar

	si_days_in_month[3] = 30;    //Apr
	si_days_in_month[4] = 31;    //May
	si_days_in_month[5] = 30;    //Jun

	si_days_in_month[6] = 31;    //Jul
	si_days_in_month[7] = 31;    //Avg
	si_days_in_month[8] = 30;    //Sep
	
	si_days_in_month[9] = 31;    //Oct
	si_days_in_month[10] = 30;   //Nov
	si_days_in_month[11] = 31;   //Dec
	
	correct_feb_days(year8_t);
	// 		31, 28, 31,
	// 		30, 31, 30,
	// 		31, 31, 30,
	// 		31, 30, 31
	//	};
}


//----------------------------------------------------------------------
//  hardware setter and getter
//----------------------------------------------------------------------
extern void _rtc_set_time(rtc_time* rt);
extern void _rtc_get_time(rtc_time* rt);
//----------------------------------------------------------------------
//  call hardware setter and correct number of days
//----------------------------------------------------------------------
void rtc_get_time(rtc_time* rt) {
	_rtc_get_time(rt);
}

//----------------------------------------------------------------------
//  call hardware setter and correct number of days
//----------------------------------------------------------------------
void rtc_set_time(rtc_time* rt) {
	_rtc_set_time(rt);
	correct_feb_days(rt->year8_t);
}

//---------------------------------------------------------------------
//  calc and update day of week
//----------------------------------------------------------------------
void rtc_refresh_dow(rtc_time* rt) {
	register uint8_t  d = rt->mday;
	register uint8_t  m = rt->mon;
	register uint16_t y = ((uint16_t)rt->year8_t) + 2000;
	
	register uint8_t dow = (d+=m<3?y--:y-2,23*m/9+d+4+y/4-y/100+y/400)%7;
	
	if (dow) {																//decode from sun, mon, tue... to mon, tue, wed...
		--dow;
	} else {
		dow = 6;
	}

	rt->wday = dow;
	
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// extern handler of aci system events
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_extern_system_event_handler() {
	
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// extern handler of aci data events
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_extern_data_event_handler(struct nrf_rx *rx) {
	
	if (!rx->length || !(rx->data[0] > 0x89 && rx->data[0] < 0x8E )) {			// exit if no events in rx buffer or there is system event
		return;
	}
	
	dp("  data");

	if(rx->data[0] == NRF_EVT_DATA_RECEIVED) {
		
		rtc_time l_rt;

		switch (rx->data[1]) {
			case PIPE_CURRENT_TIME_CURRENT_TIME_RX_ACK_AUTO:

				
				l_rt.sec  = rx->data[RX_CT_SEC];
				l_rt.min  = rx->data[RX_CT_MIN];
				l_rt.hour = rx->data[RX_CT_HOUR];
				l_rt.wday = rx->data[RX_CT_DOW];
				l_rt.mday = rx->data[RX_CT_DAY];
				l_rt.mon  = rx->data[RX_CT_MONTH];
				l_rt.year8_t = (uint8_t)((uint16_t)((rx->data[RX_CT_YEAR_HO] << 8) | rx->data[RX_CT_YEAR_LO]) - 2000);
				
				dp_val_ln("data", rx->data[2]);
				dp_val_ln("data", rx->data[3]);
				
				dp_ln("herere!");
				
				rtc_print_time(&l_rt);

				nrf_print_rx(rx);
			break;
			
			case PIPE_SI_SIMPLE_ZONE_0_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 0);
			break;

			case PIPE_SI_SIMPLE_ZONE_1_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 1);
			break;

			case PIPE_SI_SIMPLE_ZONE_2_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 2);
			break;

			case PIPE_SI_SIMPLE_ZONE_3_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 3);
			break;

			case PIPE_SI_SIMPLE_ZONE_4_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 4);
			break;

			case PIPE_SI_SIMPLE_ZONE_5_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 5);
			break;

			case PIPE_SI_SIMPLE_ZONE_6_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 6);
			break;

			case PIPE_SI_SIMPLE_ZONE_7_ZONE_CFG_RX:
				zone_config_parser(&rx->data[2], 7);
			break;

		}
		
	}

}


//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// mask and shift data service function
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t mask_and_shift(uint8_t val, uint8_t mask, uint8_t shift) {
	return (val & mask) >> shift;
}


//----------------------------------------------------------------------
//  print time for debug
//----------------------------------------------------------------------
void rtc_print_time(rtc_time *rt) {
	UARTPrintUint(rt->mday, 10);
	UARTPrint("-");
	UARTPrintUint(rt->mon, 10);
	UARTPrint("-");
	UARTPrintUint(rt->year8_t, 10);
	UARTPrint(" ");
	UARTPrintUint(rt->wday, 10);
	UARTPrint(" ");
	UARTPrintUint(rt->hour, 10);
	UARTPrint(":");
	UARTPrintUint(rt->min, 10);
	UARTPrint(":");
	UARTPrintUint(rt->sec, 10);
	UARTPrintln("");
}
//-

