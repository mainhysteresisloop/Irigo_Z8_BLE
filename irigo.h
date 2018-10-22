/*
 * irigo.h
 *
 * Created: 23.02.2018 19:17:24
 *  Author: Sergey Shelepin
  */ 


// timer 1 for main register 


#ifndef IRIGO_H_
#define IRIGO_H_

#include <stdint.h>
#include <stdbool.h>
#include "utils/si_data.h"

#define ST_EVERY_DAY			0
#define ST_DAYS_OF_WEEK			1
#define ST_ODD_DAYS				2
#define ST_EVEN_DAYS			3
#define ST_EVERY_SECOND_DAY		4
#define ST_EVERY_THIRD_DAY		5

#define DOW_MONDAY				0b00000001
#define DOW_TUESDAY				0b00000010
#define DOW_WEDNESDAY			0b00000100
#define DOW_THURSDAY			0b00001000
#define DOW_FRIDAY				0b00010000
#define DOW_SATURDAY			0b00100000
#define DOW_SUNDAY				0b01000000

// TWSE states in twse.status field
// states  model:  empty-> planned -> prepare -> active-> passed; could fo to canceled while grooming 
#define TWSE_STATE_MASK       0x07   //LSB 0-2 bits of status field
#define TWSE_STATE_NEW           0
#define TWSE_STATE_PLANNED       1
#define TWSE_STATE_PREPARE       2
#define TWSE_STATE_ACTIVE        3
#define TWSE_STATE_PASSED        4
#define TWSE_STATE_CANCELED      5

#define VALVE_OPEN_DELAY_HS      3    // 1.5 sec prepare time

#define TWSE_STATE_DOW_MASK   0xE0    // mask to get day of week from twse status field 3 MSB

//TWSE properties
#define TWSE_CORRECTED           3     // 3 bit
#define TWSE_SHIFTED             4     // 4 bit


#define MAX_ZONES				8
#define MAX_WSLOTS_PER_ZONE		3

#define ZONE_BYPASS_RAIN_DELAY	0		// zero bit
#define ZONE_BYPASS_SEASON_ADJ	1		// first bit


// zoneslot filed
#define TWSE_Z_NUM_MASK       0xFC   // 6 MSB
#define TWSE_WS_NUM_MASK      0x03   // 2 LSB




//------------------------------------------------------------------------------------------------------
// struct of element of real(true) watering schedule : 
// next TWSEA_LEN aligned items non intersecting each other. 
// that is true time of planned watering 
//------------------------------------------------------------------------------------------------------
typedef struct {
	uint8_t status;
	uint8_t zoneslot;
	uint8_t start_day;
	uint8_t start_month;
	uint8_t start_hh;
	uint8_t start_mi;
	uint8_t stop_hh;
	uint8_t stop_mi;
} true_water_schedule_element;				
#define TWSEA_LEN     10

//------------------------------------------------------------------------------------------------------
// current schedule parameters structure
//------------------------------------------------------------------------------------------------------
typedef struct {    // current setup parameters
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t dow;    // from 0 to 6
	uint8_t hh;
	uint8_t mi;
	uint8_t zone_num;
	uint8_t ws;
} current_schedule_parameters;

//------------------------------------------------------------------------------------------------------
// definition of irigo device data structures
//------------------------------------------------------------------------------------------------------
typedef struct {
	bool is_on;
	uint8_t start_hour;
	uint8_t start_min;
	uint8_t duration;      
	uint8_t start_mon;
	uint8_t start_day;
	uint8_t start_year;
	uint8_t schedule_type;
	uint8_t DoW_bitmap;
} water_slot_type;

typedef struct {
	water_slot_type water_slot[MAX_WSLOTS_PER_ZONE];
	uint8_t bypass_bitmap;								// bitmap with bypass flags
// 	bool rain_sensor_bypass;
// 	bool season_adj_bypass;
} zone_type;


typedef struct {
	uint8_t hours_left;
	uint8_t mins_left;
	uint8_t secs_left;		
} rain_pause_type;

typedef struct{
	uint8_t zone_num; // 0xFF for all zones
	uint8_t mins_left;
	uint8_t secs_left;
} manual_zone_type;

typedef struct {
	zone_type zone[MAX_ZONES];
	rain_pause_type rain_pause; 
	manual_zone_type manual_zones;
	uint8_t mode;						// AUTO or MANUAL
	uint8_t season_adj;				    // 50 - 200 %
	uint8_t	zones_status_bitmap;
	uint8_t	zones_connection_bitmap;	
} si_z8_device_type;

//------------------------------------------------------------------------------------------------------
// variables 
//------------------------------------------------------------------------------------------------------
true_water_schedule_element twsea[TWSEA_LEN];
si_z8_device_type irigo_z8;
current_schedule_parameters cur_sp;

//------------------------------------------------------------------------------------------------------
// functions 
//------------------------------------------------------------------------------------------------------
void irigo_init_z8();
void reset_cur_sp(rtc_time* rt);
bool is_any_active_ws();
void tws_rebuild();

void open_valve(uint8_t valve_num);
void close_all_valves(uint8_t valve_num);

void tws_status_processing();

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// zone config parser
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void zone_config_parser(uint8_t *cfg,  uint8_t zone_num);


#endif /* IRIGO_H_ */