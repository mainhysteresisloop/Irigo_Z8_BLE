/*
 * irigo.c
 *
 * Created: 26.02.2018 12:44:45
 *  Author: USER
 */ 


#include "irigo.h"
#include <string.h>

#ifdef DEBUG_L
#include "utils/UARTSI.h"
#include "utils/si_debug.h"
#include <avr/pgmspace.h>
#endif

#ifdef DEBUG_L
void tws_print();
void print_water_config();
#endif

typedef uint8_t zone_status; // logic type for zone status: 0 - all off, > 0  - on : active zone = zone_status -1 



inline void run_timer1_at_500msec() {
	OCR1A  = 16625; //31249;           // hopefully compiler do this 16-bit assignment :)
	TIMSK1 = _BV(OCIE1A);
	TCCR1B = _BV(WGM12) | _BV(CS12);   // run at CTC mode with prescaler = 256
	
}

void irigo_init_z8() {
	
	run_timer1_at_500msec();
	
}

//-----------------------------------------------------------------------------------
//  removes first queue passed and canceled tasks from tws
//  return true if records were removed
//-----------------------------------------------------------------------------------
uint8_t irigo_tws_cleanup() {
	
	register uint8_t sf = true;
	register uint8_t res = false;
	
	while(sf) {
		
		sf = false;
		
		uint8_t twse_state = twsea[0].status & TWSE_STATE_MASK;
		
		if(twse_state == TWSE_STATE_PASSED || twse_state == TWSE_STATE_CANCELED) {
			memcpy(twsea, twsea + 1, sizeof(twsea)*(TWSEA_LEN - 1)/TWSEA_LEN);
			twsea[TWSEA_LEN - 1].status = 0;
			sf = true;
			res = true;
		}
	}
	
	return res;
}


//-----------------------------------------------------------------------------------
// incrementing time in cur_sp structure. step is 5 min
//-----------------------------------------------------------------------------------
void increment_cur_sp_time() {
	
	const uint8_t inc = 5;														// 5 min increment  only 1, 2, 3, 4, 5, 10, 12, 15, 20, 30 are ok
	
	if (cur_sp.mi != (60-inc)) {
		cur_sp.mi += inc;
		} else {																// new hour
		cur_sp.mi = 0;
		
		if(cur_sp.hh != 23){
			++cur_sp.hh;
			} else {															// new day
			cur_sp.hh = 0;
			
			if (cur_sp.dow == 6) {												// cycle day of week
				cur_sp.dow = 0;
				} else{
				++cur_sp.dow;
			}
			
			if(cur_sp.day != si_days_in_month[cur_sp.month - 1]) {
				++cur_sp.day;
				} else {														// new month
				cur_sp.day = 1;
				if (cur_sp.month !=12)	{
					++cur_sp.month;
					} else {													// new year
					cur_sp.month = 1;
					++cur_sp.year;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------
// incrementing cur_sp starting form water slots
//-----------------------------------------------------------------------------------
void increment_cur_sp() {
	if(cur_sp.ws < MAX_WSLOTS_PER_ZONE - 1) {
		++cur_sp.ws;
		} else {
		cur_sp.ws = 0;
		if(cur_sp.zone_num < MAX_ZONES - 1 ) {        // new zone
			++cur_sp.zone_num;
			} else {
			cur_sp.zone_num = 0;
			increment_cur_sp_time();         // new time
		}
	}
}

//-----------------------------------------------------------------------------------
//  reset cur_sp based on current time (rt)
//-----------------------------------------------------------------------------------
void reset_cur_sp(rtc_time* rt) {
	
	cur_sp.year = rt->year8_t;
	cur_sp.month = rt->mon;
	cur_sp.day = rt->mday;
	cur_sp.dow = rt->wday;
	cur_sp.hh = rt->hour;
	cur_sp.mi = rt->min;
	cur_sp.zone_num = 0;
	cur_sp.ws = 0;
	
	do {                                                                       // adjust cur_sp minutes to 5 min quantization
		++cur_sp.mi;
	} while (cur_sp.mi%5 != 0);
	
	if(cur_sp.mi > 55) {
		cur_sp.mi = 55;
		increment_cur_sp_time();
	}
	
}

//-----------------------------------------------------------------------------------
//  checks if there any active water slot configured,
//-----------------------------------------------------------------------------------
bool is_any_active_ws() {
	for (uint8_t z = 0; z < MAX_ZONES; z++) {
		for(uint8_t s = 0; s < MAX_WSLOTS_PER_ZONE; s++) {
			
			if(irigo_z8.zone[z].water_slot[s].is_on) {
				dp_ln("hhh");
				return true;
			}
			
// 			if (wca[z].ws[s].is_on)	{
// 				return true;
// 			}
		}
	}
	
	return false;
	
}

//-----------------------------------------------------------------------------------
// checking wce start based on cur_sp data
//-----------------------------------------------------------------------------------
uint8_t check_wce_start_at_cur_sp() {
	
	register uint8_t z = cur_sp.zone_num;
	register uint8_t s = cur_sp.ws;

	water_slot_type *p = &irigo_z8.zone[z].water_slot[s];
	
	if(p->schedule_type == ST_DAYS_OF_WEEK) {							// if schedule type is day of weeks
		if(!(p->DoW_bitmap & _BV(cur_sp.dow))) {						// in case if cur_sp.dow doesn't match the DoW_bitmap
			return false;												// return false
		}
	}
	
	return
	p->is_on && 
	p->start_hour == cur_sp.hh && 
	p->start_min ==  cur_sp.mi;

}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int16_t get_diff_in_min(uint8_t next_hh, uint8_t next_mi, uint8_t prev_hh, uint8_t prev_mi ) {
	return (int16_t)(next_hh - prev_hh)*60 + (int16_t)(next_mi - prev_mi);
}

//-----------------------------------------------------------------------------------
// aligns 1 part;
// input: first member number
//-----------------------------------------------------------------------------------
void align_pair_twe(uint8_t fmn) {
	register uint8_t prev_stop_hh  = twsea[fmn].stop_hh;
	register uint8_t prev_stop_mi  = twsea[fmn].stop_mi;
	register uint8_t next_start_hh = twsea[fmn + 1].start_hh;
	register uint8_t next_start_mi = twsea[fmn + 1].start_mi;
	
	//	int16_t minutes_diff = (int16_t)(next_start_hh - prev_stop_hh)*60 + (int16_t)(next_start_mi - prev_stop_mi);
	int16_t minutes_diff = get_diff_in_min(next_start_hh, next_start_mi, prev_stop_hh, prev_stop_mi);
	
	if (minutes_diff < 0) {
		
		uint16_t new_min = (uint16_t)next_start_mi - minutes_diff;
		
		twsea[fmn + 1].start_hh += new_min / 60;
		twsea[fmn + 1].start_mi = new_min % 60;
		
		new_min = (uint16_t)twsea[fmn + 1].stop_mi - minutes_diff;

		twsea[fmn + 1].stop_hh += new_min / 60;;
		twsea[fmn + 1].stop_mi = new_min % 60;;
		
		twsea[fmn + 1].status |= _BV(TWSE_SHIFTED);                                     // put is_shifted flag
		
	}

}

//-----------------------------------------------------------------------------------
// tws grooming.
// assuming ALL items are FILLED(!)
//-----------------------------------------------------------------------------------
void groom_tws() {
	
	// 1. alignment
	for (uint8_t i = 0; i < TWSEA_LEN - 1; i++) {                                      // align all twse in TWS cycle
		
		if( twsea[i + 1].start_month == twsea[i].start_month &&                         // if same start day and (to be on the safe side) month
		twsea[i + 1].start_day == twsea[i].start_day) {
			align_pair_twe(i);                                                     // align that pair
		}
		
	}
	
	#ifdef DEBUG_L
	print_water_config();
	UARTPrintln("");
	UARTPrintln("after alignment");
	tws_print();
	#endif
	
	//2. correct or cancel some twse
	for (uint8_t i = 0; i < TWSEA_LEN; i++) {												// align all twse in TWS cycle
		
		if(twsea[i].start_hh >= 24 ) {													// 1. could not start in the next day - cancel watering
			
			twsea[i].status &= ~TWSE_STATE_MASK;                                        // clear 3 lsb states bits
			twsea[i].status |= TWSE_STATE_CANCELED;
			
		} else if(twsea[i].stop_hh >= 24 ) {											// 2. could not move over the day - correct watering
			twsea[i].stop_hh = 23;
			twsea[i].stop_mi = 59;
			twsea[i].status |= _BV(TWSE_CORRECTED);
		}
	}
	#ifdef DEBUG_L
	UARTPrintln("after cancel/correct");
	tws_print();
	#endif
	
	
}

//-----------------------------------------------------------------------------------
// Do all necessary calculations 
//-----------------------------------------------------------------------------------
void autorun_handler(true_water_schedule_element *tws_ea, zone_status *z_s) {
	
	
	
}

//-----------------------------------------------------------------------------------
// returns duration zone/ws  duration adjusted on season adj
//-----------------------------------------------------------------------------------
//uint8_t get_duration(uint8_t zone_num, uint8_t ws, uint8_t season_adj ) {
uint8_t get_duration(uint8_t zone_num, uint8_t ws) {
	return irigo_z8.zone[cur_sp.zone_num].water_slot[cur_sp.ws].duration;			  //@todo:  ref to irigo_z8 obj by pointer
}
//-----------------------------------------------------------------------------------
// copy current schedule parameters to true water schedule element
//-----------------------------------------------------------------------------------
void copy_cur_schedule_parameters_to_twse(true_water_schedule_element *e, current_schedule_parameters *c) {
	e->start_month = c->month;
	e->start_day = c->day;
	e->start_hh = c->hh;
	e->start_mi = c->mi
	e->zoneslot = c->zone_num << 2;
	e->zoneslot |= c->ws;
	e->status = (c->dow << 5);														 // put dow into status

	register uint8_t dur = irigo_z8.zone[c->zone_num].water_slot[c->ws].duration;	 // @todo: add duration to current_schedule_parameters struct
	
	register uint8_t mi = cur_sp.mi + dur;											 // simple calc and update of stop hour and stop minute
	e->stop_hh = cur_sp.hh + mi/60;
	e->stop_mi = mi%60;	
	
 }
	
//-----------------------------------------------------------------------------------
// Build tws based on watering config array (wca) and current setup parameters (cur_sp)
// replacing elements with NEW status (!twsea[i].status & TWSE_STATE_MASK).
// Need grooming afterwards.
//-----------------------------------------------------------------------------------
void tws_rebuild(true_water_schedule_element *tws_ea, current_schedule_parameters * csp) {
	
	if(!is_any_active_ws()) {
		return;
	}
	dp_ln("!here");
	
	register uint8_t tws_changed = false;
	
	for (uint8_t i = 0; i < TWSEA_LEN; i++) {															    // checking all the array of twse
//		if(!(twsea[i].status & TWSE_STATE_MASK)) {													    	// if twse is new - assuming TWSE_STATE_NEW is 0
		
		if(!(tws_ea->status & TWSE_STATE_MASK)) {													    	// if twse is new - assuming TWSE_STATE_NEW is 0
			
			uint8_t mf = false;
			while (!mf) {																				    // get zone and ws og
				mf = check_wce_start_at_cur_sp();
				if (mf) {
					copy_cur_schedule_parameters_to_twse(tws_ea, csp);
					tws_ea->status |= TWSE_STATE_PLANNED;													// set planned state
					register uint8_t mi = csp->mi + get_duration(csp->zone_num, csp->ws);					// simple calc and update of stop hour and stop minute
					tws_ea->stop_hh = csp->hh + mi/60;
					tws_ea->stop_mi = mi%60;
					
					tws_changed = true;
					
				}
				increment_cur_sp();
			}
			++tws_ea;
		}
		
	}
	#ifdef DEBUG_L
	dp_ln(""); 
	dp_ln("tws rebuild:");
	tws_print();
	#endif
	
	if(tws_changed) {
		groom_tws();
	}
	
}


//-----------------------------------------------------------------------------------
//  removes first queue passed and canceled tasks from tws
//  return true if records were removed
//-----------------------------------------------------------------------------------
uint8_t tws_cleanup(true_water_schedule_element *tws_ea) {
	
	register uint8_t sf = true;
	register uint8_t res = false;
	
	while(sf) {
		

		UARTPrintValDec_P(PSTR("sizeof(twse)="), sizeof(true_water_schedule_element)); 
		UARTPrintln("");
		UARTPrintValDec_P(PSTR("sizeof(tws_ea)="), sizeof(tws_ea));
		UARTPrintln("");
		
		sf = false;
		
//		uint8_t twse_state = twsea[0].status & TWSE_STATE_MASK;
		uint8_t twse_state = tws_ea->status & TWSE_STATE_MASK;
		
		if(twse_state == TWSE_STATE_PASSED || twse_state == TWSE_STATE_CANCELED) {
			memcpy(tws_ea, tws_ea + 1, sizeof(true_water_schedule_element)*(TWSEA_LEN - 1));
			twsea[TWSEA_LEN - 1].status = TWSE_STATE_NEW;
			sf = true;
			res = true;
		}
	}
	
	return res;
}

//-----------------------------------------------------------------------------------
// switching statuses of tws elements base on time
//-----------------------------------------------------------------------------------
void tws_status_loop(true_water_schedule_element *tws_ea, rtc_time *rt) {
	
//	true_water_schedule_element* e = (true_water_schedule_element*)&twsea;                    // make pointer to first twsea element

	true_water_schedule_element *e = tws_ea;												  // make pointer to first twsea element
	
	if (e->start_day != rt->mday || e->start_month != rt->mon) {                                // consider only today actions
		return;
	}
	
	//	int16_t minutes_diff = (int16_t)(next_start_hh - prev_stop_hh)*60 + (int16_t)(next_start_mi - prev_stop_mi);
	
	switch (e->status & TWSE_STATE_MASK) {
		case TWSE_STATE_PLANNED:
			if(get_diff_in_min(rt->hour, rt->min, e->start_hh, e->start_mi) >= 0){
				e->status &= ~TWSE_STATE_MASK;											     // could be coded as e->status++
				e->status |= TWSE_STATE_ACTIVE;
			}
		break;

		case TWSE_STATE_ACTIVE:
			if(get_diff_in_min(rt->hour, rt->min, e->stop_hh, e->stop_mi) >= 0) {
				e->status &= ~TWSE_STATE_MASK;												 // could be coded as e->status++
				e->status |= TWSE_STATE_PASSED;
			}
		break;

		case TWSE_STATE_PASSED:
		case TWSE_STATE_CANCELED:
		case TWSE_STATE_NEW:
		default:
		
		break;
	}
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// water slot config parser
// ws_num - starts from 0
// *cfg - pointer to zone configuration string
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void water_slot_config_parser(uint8_t *cfg, uint8_t zone_num, uint8_t ws_num) {
	
	cfg += RX_SZ_SRV_DATA_LEN - (ws_num +1)*RX_SZ_SRV_WS_DATA_LEN;							// calc pointer to required water slot 

	dp_ln("in wscp wsd");
	for (uint8_t i = 0; i < RX_SZ_SRV_WS_DATA_LEN; i++) {
		UARTPutChar(' ');
		UARTPrintUint_M(*(cfg +i), 16, 2);
	}
	dp_ln("");
	
	irigo_z8.zone[zone_num].water_slot[ws_num].is_on		 = mask_and_shift( *(cfg + WS_START_HOUR_AND_SLOT_ON_OFF), WS_IS_ON_OFF_MASK, WS_IS_ON_OFF_SHIFT );
	irigo_z8.zone[zone_num].water_slot[ws_num].schedule_type = mask_and_shift( *(cfg + WS_TYPE_AND_START_MONTH), WS_SCHEDULE_TYPE_MASK, WS_SCHEDULE_TYPE_SHIFT );
	irigo_z8.zone[zone_num].water_slot[ws_num].start_min     = *(cfg + WS_START_MIN);
	irigo_z8.zone[zone_num].water_slot[ws_num].start_hour    = mask_and_shift( *(cfg + WS_START_HOUR_AND_SLOT_ON_OFF), WS_START_HOUR_MASK, WS_START_HOUR_SHIFT );
	irigo_z8.zone[zone_num].water_slot[ws_num].start_day     = *(cfg + WS_START_DAY_OR_DOW);
	irigo_z8.zone[zone_num].water_slot[ws_num].start_mon     = mask_and_shift( *(cfg + WS_TYPE_AND_START_MONTH), WS_START_MONTH_MASK, WS_START_MONTH_SHIFT );
	irigo_z8.zone[zone_num].water_slot[ws_num].start_year    = *(cfg + WS_START_YEAR);
	irigo_z8.zone[zone_num].water_slot[ws_num].DoW_bitmap    = *(cfg + WS_START_DAY_OR_DOW);
	irigo_z8.zone[zone_num].water_slot[ws_num].duration      = *(cfg + WS_DURATION);	
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// zone config parser
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void zone_config_parser(uint8_t *cfg, uint8_t zone_num) {
	
	dp_ln("in zcp");
	
	    for (uint8_t i = 0; i < RX_SZ_SRV_DATA_LEN; i++) {
		    UARTPutChar(' ');
		    UARTPrintUint_M(*(cfg +i), 16, 2);
	    }
	dp_ln("");
	
	irigo_z8.zone[zone_num].bypass_bitmap = *(cfg + RX_SZ_ZONE_CFG);							// writing zone configuration bitmap
	
	for(uint8_t i = 0; i < RX_SZ_SRV_WS_QTY; i++) {
		water_slot_config_parser(cfg, zone_num, i);							// parsing each water slot
		dp_val_ln("ws = ", i);
	}
	
	print_water_config();	
	
}

//-----------------------------------------------------------------------------------
// debug print
//-----------------------------------------------------------------------------------
#ifdef DEBUG_L
//-----------------------------------------------------------------------------------
// true watering schedule debug print
//-----------------------------------------------------------------------------------

void tws_print() {
	
	UARTPrintln_P(PSTR("TWSEA:"));
	for(uint8_t i = 0; i < TWSEA_LEN; i++) {
		for (uint8_t j = 0; j < 8; j ++) {
			UARTPrintUint( *((uint8_t*)twsea + j + i*8), 10) ;
			UARTPrint(" ");
		}
		
		UARTPrint(" |  ");
		uint8_t tt = *((uint8_t*)twsea + i*8 + 1);
		UARTPrintUint(tt  >> 2, 10);
		UARTPrint(" ");
		UARTPrintUint((tt & TWSE_WS_NUM_MASK), 10);
		UARTPrint(" ");

		tt = *((uint8_t*)twsea + i*8 );
		register uint8_t ch = 33;
		
		switch (tt & TWSE_STATE_MASK){
			case TWSE_STATE_NEW:
			ch = 'N';
			break;
			case TWSE_STATE_PLANNED:
			ch = 'P';
			break;
			case TWSE_STATE_PREPARE:
			ch = 'R';
			break;
			case TWSE_STATE_ACTIVE:
			ch = 'A';
			break;
			case TWSE_STATE_PASSED:
			ch = 'D';
			break;
			case TWSE_STATE_CANCELED:
			ch = 'N';
			UARTPrint_P(PSTR("CA"));
			break;
		}
		UARTPutChar(ch);
		UARTPrint(" ");
		if (tt & _BV(TWSE_SHIFTED)) {
			UARTPrint("S");
			UARTPrint(" ");
		}
		if (tt & _BV(TWSE_CORRECTED)) {
			UARTPrint_P(PSTR("COR"));
			UARTPrint(" ");
		}
		UARTPrintln("");
	}
	
}

//-----------------------------------------------------------------------------------
// watering config debug print
//-----------------------------------------------------------------------------------
void print_water_config() {
	UARTPrintln("");
	UARTPrintln_P(PSTR("WATER CONFIG:"));
	
	for(uint8_t i = 0; i < MAX_ZONES; i++) {
		UARTPrintln("");
		UARTPrintValDec("ZONE ", i);
// 		UARTPrintValDec(" Valve=", wca[i].valve);
// 		UARTPrintValDec(" rc.type=", wca[i].rc.type);
// 		UARTPrintlnValDec(" rc.data=", wca[i].rc.data);

		UARTPrintln("");
		
		for (uint8_t j = 0; j < MAX_WSLOTS_PER_ZONE; j++) {
			UARTPrint_P(PSTR(" ->wslot "));
			UARTPrintUint(j, 10);
			UARTPrint(": ");
			UARTPrintValDec("is_on=", irigo_z8.zone[i].water_slot[j].is_on);
			UARTPrintValDec(" sh_type=", irigo_z8.zone[i].water_slot[j].schedule_type);
			UARTPrint_P(PSTR(" dow_bm ="));
			UARTPrintUintBinary(irigo_z8.zone[i].water_slot[j].DoW_bitmap);							
			UARTPrint("  START:");
			UARTPrintValDec(" year=", irigo_z8.zone[i].water_slot[j].start_year);
			UARTPrintValDec(" mon=", irigo_z8.zone[i].water_slot[j].start_mon);			
			UARTPrintValDec(" day=", irigo_z8.zone[i].water_slot[j].start_day);
			UARTPrintValDec(" hour=", irigo_z8.zone[i].water_slot[j].start_hour);			
			UARTPrintValDec(" min=", irigo_z8.zone[i].water_slot[j].start_min);
			UARTPrintlnValDec(" dur=", irigo_z8.zone[i].water_slot[j].duration);
		}
		
	}
}

#endif

	bool is_on;
	uint8_t start_hour;
	uint8_t start_min;
	uint8_t duration;
	uint8_t start_mon;
	uint8_t start_day;
	uint8_t start_year;
	uint8_t schedule_type;
	uint8_t DoW_bitmap;
