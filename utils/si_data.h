/*
 * si_data_types.h
 *
 * Created: 26.02.2018 14:05:11
 *  Author:  Sergey Shelepin
 */ 


#ifndef SI_DATA_H_
#define SI_DATA_H_

#include <stdint.h>

#include "../nrf/nrf.h"
#include "si_debug.h"


//------------------------------------------------------------------------------------------------------BLE servicesfields---------------------------------------------------------------------------------------------------
//----- BLE CURRENT TIME service  --------
// format LSO
// year: uint16
// month: uint8  0 - Month not known, 1-Jan, 12- Dec
// day of month: uint8 0 - day not known
// hours: uint8
// min: uint8
// sec: uint8
// day of week: uint8  0 - not known, 1- Mon, 7 - Sun
// fraction: uint8
// adjust reason: uint8
#define RX_CT_ADJ_REASON   2
#define RX_CT_FRACTION     3
#define RX_CT_DOW		   4
#define RX_CT_SEC		   5
#define RX_CT_MIN		   6
#define RX_CT_HOUR		   7
#define RX_CT_DAY		   8
#define RX_CT_MONTH		   9
#define RX_CT_YEAR_HO	  10		// high octet
#define RX_CT_YEAR_LO	  11		// low  octet

//----- BLE SI SIMPLE ZONE service  --------
#define RX_SZ_SRV_DATA_LEN					       19  // 19 bytes length
#define RX_SZ_SRV_WS_QTY					        3  // total number of ws per zone
#define RX_SZ_SRV_WS_DATA_LEN				        6  // data len of one ws slot

#define WS_IS_ON_OFF_MASK						 0x20
#define WS_IS_ON_OFF_SHIFT							5
#define WS_SCHEDULE_TYPE_MASK					 0x07
#define WS_SCHEDULE_TYPE_SHIFT						0
#define WS_START_MONTH_MASK				    	 0x78
#define WS_START_MONTH_SHIFT						3
#define WS_START_HOUR_MASK		                 0x1F
#define WS_START_HOUR_SHIFT				            0

#define RX_SZ_ZONE_CFG								0
#define RX_SZ_SLOT_2_START_YEAR						1
#define RX_SZ_SLOT_2_START_DAY_OR_DOW				2
#define RX_SZ_SLOT_2_SCH_TYPE_AND_START_MONTH       3
#define RX_SZ_SLOT_2_DURATION						4
#define RX_SZ_SLOT_2_START_MIN						5
#define RX_SZ_SLOT_2_START_HOUR_AND_SLOT_ON_OFF		6
#define RX_SZ_SLOT_1_START_YEAR						7
#define RX_SZ_SLOT_1_START_DAY_OR_DOW				8
#define RX_SZ_SLOT_1_SCH_TYPE_AND_START_MONTH       9
#define RX_SZ_SLOT_1_DURATION					   10
#define RX_SZ_SLOT_1_START_MIN					   11
#define RX_SZ_SLOT_1_START_HOUR_AND_SLOT_ON_OFF    12
#define RX_SZ_SLOT_0_START_YEAR					   13 
#define RX_SZ_SLOT_0_START_DAY_OR_DOW              14
#define RX_SZ_SLOT_0_SCH_TYPE_AND_START_MONTH      15
#define RX_SZ_SLOT_0_DURATION					   16
#define RX_SZ_SLOT_0_START_MIN					   17
#define RX_SZ_SLOT_0_START_HOUR_AND_SLOT_ON_OFF    18

#define WS_START_YEAR								0
#define WS_START_DAY_OR_DOW							1
#define WS_TYPE_AND_START_MONTH						2
#define WS_DURATION									3
#define WS_START_MIN								4
#define WS_START_HOUR_AND_SLOT_ON_OFF				5




//------------------------------------------------------------------------------------------------------HAL for RTC------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------
// si real time data structure 
// DO NOT TOUCH THE SEQUENCE - it's optimized for PCF8583 RTC chip
//------------------------------------------------------------------------------------------------------
typedef struct {
	uint8_t sec;      // 0 to 59
	uint8_t min;      // 0 to 59
	uint8_t hour;     // 0 to 23
	uint8_t wday;     // 1-7
	uint8_t mday;     // 1 to 31
	uint8_t mon;      // 1 to 12
	uint8_t year8_t;  // year - starting from 2000
} rtc_time;

//------------------------------------------------------------------------------------------------------
// wrappers for hardware set and get methods
//------------------------------------------------------------------------------------------------------
void rtc_set_time(rtc_time* rt);
void rtc_get_time(rtc_time* rt);
//------------------------------------------------------------------------------------------------------
// rtc print time function
//------------------------------------------------------------------------------------------------------
void rtc_print_time(rtc_time* rt);

void rtc_refresh_dow(rtc_time* rt);
//------------------------------------------------------------------------------------------------------
// array with number of days in each month
//------------------------------------------------------------------------------------------------------
uint8_t si_days_in_month[12];

void si_set_month_days(uint8_t year8_t);

//------------------------------------------------------------------------------------------------------ end of HAL for RTC------------------------------------------------------------------------------------------------

//----- data service functions--------------------------------------------------------------------------
uint8_t mask_and_shift(uint8_t val, uint8_t mask, uint8_t shift);

//------------------------------------------------------------------------------------------------------
// extern handler of aci system events
//------------------------------------------------------------------------------------------------------
void nrf_extern_data_event_handler();
void nrf_extern_system_event_handler();

#endif /* SI_DATA_H_ */