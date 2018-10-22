/*
 * si_debug.h
 *
 * Created: 26.02.2018 17:47:50
 *  Author: USER
 */ 


#ifndef SI_DEBUG_H_
#define SI_DEBUG_H_

#ifdef DEBUG_L
#define dp_ln(x)				UARTPrintln(x)
#define dp(x)					UARTPrint(x)
#define dp_ln_p(x)				UARTPrintln_P(PSTR(x))
#define dp_val_ln(name, val)    UARTPrintlnValDec(name, val)

#define dp_time(x)				rtc_print_time(x)

#else
#define dp_ln(x)				
#define dp(x)					
#define dp_ln_p(x)				
#define dp_val_ln(name, val)

#define dp_time(x)				

#endif




#endif /* SI_DEBUG_H_ */