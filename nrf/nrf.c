/*
 * nRF8001 handling code
 * Part of the Bluetooth LE example system
 *
 * Copyright 2017 Sven Gregori
 * Released under MIT License
 *
 */
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "../utils/UARTSI.h"
#include "../utils/SPIsi.h"
#include "../utils/si_data.h"
#include "../utils/si_debug.h"
#include "nrf.h"
#include "services.h"
#include <stdbool.h>
#include <avr/eeprom.h>


#define EEPROM_DD_STATUS_ADDRESS  0								// eeprom address of dynamic data status byte, next byte is start byte of DD array in eeprom 
#define EEPROM_DD_START_OFFSET    EEPROM_DD_STATUS_ADDRESS + 1	// first byte off eeprom array
#define EEPROM_OK_STATUS_MASK     0x40							// 01 higher bit mask
#define EEPROM_ERROR_STATUS       0

extern void nrf_extern_system_event_handler();
extern void nrf_extern_data_event_handler();

/* String constants stored in PROGMEM */
static const char string_pipe_closed[] PROGMEM  = "Pipe not open\r\n";
static const char string_connection[] PROGMEM   = "Connection from: ";
static const char string_received[] PROGMEM     = "Received unhandled data: ";
static const char string_temperature[] PROGMEM  = "Temperature: ";
static const char string_celsius[] PROGMEM      = " C\r\n";

/* Global rx data structure */
struct nrf_rx rx;

bool is_bond_info_stored;																									// 
bool is_bonded;																											// is bonded flag 
static uint8_t nrf_data_credits;
//static uint8_t opmode;
//static uint8_t pipes;
static uint64_t pipes_open;
static struct nrf_tx tx;
static const struct service_pipe_mapping service_pipe_map[] = SERVICES_PIPE_TYPE_MAPPING_CONTENT;
static const struct nrf_setup_data setup_data[NB_SETUP_MESSAGES] PROGMEM = SETUP_MESSAGES_CONTENT;

void print_event_name(uint8_t evt_code);
void eeprom_test_read();

bool nrf_store_bond_data();
bool restore_bond_data();
bool nrf_restore_dynamic_data(uint8_t eeprom_status_byte);

void nrf_pins_setup() {    
	
	BLE_REQN_DDR  |= _BV(BLE_REQN_PIN);			// output
	BLE_RESET_DDR |= _BV(BLE_RESET_PIN);		// output
	
	BLE_RDYN_PORT |= _BV(BLE_RDYN_PIN);			// input (DDR bit is zero) with pull-up (port bit is 1)

}

/**
 * Reset the nRF8001 module.
 *
 * Pulls the module's reset pin low and waits for a moment. Afterwards
 * nrf_setup() is called, which will set the pin high again.
 *
 * The return value from nrf_setup() is returned.
 *
 * @param none
 * @return 0 on success, a negative value in case of an error (see nrf_setup())
 */
void nrf_reset_module(void) {
    led_setup_off();
    led_connect_off();

    ble_reset_low();
    _delay_ms(10);
	nrf_start();
}

void nrf_start() {
    ble_reset_high();
    _delay_ms(100);		// 62ms according DS, 100ms to be on the safe side
}

/**
 * Setup nRF8001 module.
 *
 * Send all setup data generated from nRFgo Studio in nrf/services.h to the
 * module via SPI and take care that everything is set up properly.
 *
 * If anything goes wrong during the setup phase, the setup process is
 * aborted and the module will not be functional. A negative return value
 * will indicate an error.
 *
 * @param none
 * @return 0 on success, a negative value in case of an error.
 */
int8_t nrf_setup(void) {
	
    uint8_t cnt;

    /* Send all setup data to nRF8001 */
    for (cnt = 0; cnt < NB_SETUP_MESSAGES; cnt++) {
        memcpy_P(&tx, &setup_data[cnt].data, sizeof(struct nrf_tx));
        nrf_transmit(&tx, &rx);

        if (rx.length == 0) {
            nrf_print_rx(&rx);
            continue;
        }
        nrf_print_rx(&rx);

        /* Make sure transaction continue command response event is received */
        if (rx.data[0] != NRF_EVT_CMD_RESPONSE ||
            rx.data[1] != NRF_CMD_SETUP ||
            rx.data[2] != ACI_STATUS_TRANSACTION_CONTINUE)
        {
            return -3;
        }
    }
    
    /* Receive all setup command response events */
    do {
        nrf_receive(&rx);
        nrf_print_rx(&rx);
    } while (rx.data[0] == NRF_EVT_CMD_RESPONSE &&
             rx.data[1] == NRF_CMD_SETUP &&
             rx.data[2] == ACI_STATUS_TRANSACTION_CONTINUE);

    /* Make sure transaction complete command response event is received */
    if (rx.data[0] != NRF_EVT_CMD_RESPONSE ||
        rx.data[1] != NRF_CMD_SETUP ||
        rx.data[2] != ACI_STATUS_TRANSACTION_COMPLETE)
    {
        return -4;
    }

    led_setup_on();
    
    return true;
}


/**
 * Start non-connectible advertising
 *
 * @param none
 * @return none
 */
void nrf_cmd_broadcast(void) {
	
    data16_t timeout;
    data16_t advival;

    memset(&tx, 0, sizeof(tx));

    timeout.word = 600;
    advival.word = 0x0100;

    tx.length = 5;
    tx.command = NRF_CMD_BROADCAST;
    /* send LSB first */
    tx.data[0] = timeout.lsb;
    tx.data[1] = timeout.msb;
    tx.data[2] = advival.lsb;
    tx.data[3] = advival.msb;

    nrf_send(&tx);
}



/**
 * Start advertising, waiting for remote side to connect.
 *
 * @param 
 run_timeout: Timeout 0x0000 Infinite advertisement, no timeout
			  If required, the RadioReset command will abort the continuous advertisement and return nRF8001 to Standby mode
			  1-16383 (0x3FFF) Valid timeout range in seconds
 adv_interval: AdvInterval 32 - 16384  (0x0020 to 0x4000) Advertising interval set in periods of 0.625 msec
 * @return none
 */
void nrf_cmd_connect(uint16_t run_timeout, uint16_t adv_interval) {

    memset(&tx, 0, sizeof(tx));

    tx.length = MSG_CONNECT_LEN;
    tx.command = NRF_CMD_CONNECT;

    /* send LSB first. */
    tx.data[0] = run_timeout; 
    tx.data[1] = run_timeout >> 8; 
    tx.data[2] = adv_interval; 
    tx.data[3] = adv_interval >> 8;
	
	dpl_p(PSTR("CMD->: connect"));

    nrf_send(&tx);
}

/**
 * Start advertising, waiting for remote side to bond.
 *
 * @param 
	 run_timeout: Timeout 1-180  (0x0001 – 0x00B4) Valid advertisement timeout range in seconds
	adv_interval: AdvInterval 0x0020 to 0x4000 Advertising interval set in periods of 0.625 msec (LSB/MSB)
 * @return none
 */
void nrf_cmd_bond(uint16_t run_timeout, uint16_t adv_interval) {

    memset(&tx, 0, sizeof(tx));

    tx.length = MSG_BOND_LEN;
    tx.command = NRF_CMD_BOND;

    /* send LSB first. */
    tx.data[0] = run_timeout; 
    tx.data[1] = run_timeout >> 8; 
    tx.data[2] = adv_interval; 
    tx.data[3] = adv_interval >> 8;

	dpl_p(PSTR("CMD->: bond"));
    nrf_send(&tx);
}



/*
 * ACI pipe closing handling
 *
 * There is no PipeStatusEvent on disconnect, so pipes opened by a remote
 * client (most likely notification pipes) have to be manually removed
 * from the pipes_open variable.
 *
 * This will be handled by using the nrf_tx_pipe_map bitfield that is
 * initialized during start to contain all pipe numbers that are defined
 * as local storage and TX pipe. Once a disconnect event is received, the
 * internal pipes_open bitfield is adjusted with it.
 */

/** bitfield of ACI pipes to be closed on remote disconnect */
uint64_t nrf_tx_pipe_map;

/**
 * Set up nrf_tx_pipe_map bitfield.
 *
 * All pipes defined as ACI_STORE_LOCAL and ACI_TX are added to the bitfield.
 *
 * @param none
 * @return none
 */
void nrf_tx_map_pipes(void) {
    uint8_t i;

    for (i = 0; i < NUMBER_OF_PIPES; i++) {
        if (service_pipe_map[i].store == ACI_STORE_LOCAL &&
            service_pipe_map[i].type == ACI_TX)
        {
            nrf_tx_pipe_map |= 1 << (i + 1);
        }
    }
}

/**
 * Internally close all ACI pipes.
 *
 * Pipes that where opened by a remote client and not the nRF8001 module
 * need to be closed manually.
 *
 * @param none
 * @return none
 */
void nrf_close_tx_pipes(void) {
    uint8_t i;

    pipes_open &= ~(nrf_tx_pipe_map);
    dpl_p(PSTR("Open Pipes: "));
    for (i = 1; i <= NUMBER_OF_PIPES; i++) {
        if (pipes_open & (1 << i)) {
            UARTPrintUint(i, 10);
            UARTPutChar(' ');
        }
    }
    dp_new_line();
}

/* Dummy tx and rx data structures */
static struct nrf_tx dummy_tx;
static struct nrf_rx dummy_rx;

/**
 * nRF8001 transmission function.
 * Send and simultaniously receive data to and from the nRF8001 module.
 * Note, received data is always related to a previous transmission or
 * an otherwise asynchronous event, but will never be the direct result of
 * the call in progress. If data is expected to be sent from the nRF8001
 * module afterwards, a subsequent transmit call has to be made in order to
 * receive the data. But keep in mind that asynchronous data might be sent,
 * so ignoring the received data during a send-only operation is not
 * recommended.
 *
 * In cases where received data really can be safely ignored, or no data
 * has to be trasmitted in the first place, but only received, NULL values
 * can be given for the tx and rx parameter respectively. In addition, two
 * macros exist as a shortcut for these cases:
 *      nrf_send(struct nrf_tx *)       for send-only
 *      nrf_receive(struct nrf_rx *)    for receive-only
 *
 * In addition, a third macro, nrf_txrx(struct nrf_tx *, struct nrf_rx *)
 * exists to have a sequential send and receive operation, i.e calling
 * a send-only nrf_transmit() followed directly by a receive-only one.
 *
 * @param tx nrf_tx structure for sending, make sure the data buffer is
 *           filled with zeroes. Can be NULL
 * @param rx nrf_rx structure for receiving, can be NULL
 * @return 0
 *
 */
int8_t nrf_transmit(struct nrf_tx *tx, struct nrf_rx *rx) {
	
    uint8_t i;

    reqn_set_low();
    while (rdyn_is_high()) {
        /* wait */
    }

    /*
     * Check if given tx struct is NULL and only rx is of interest.
     * Use global dummy_tx struct (all fields zero) for sending.
     */
    if (tx == NULL) {
        tx = &dummy_tx;
    }

    /*
     * Check if given rx struct is NULL and only tx is of interest.
     * Receive into global dummy_rx structure and ignore it.
     */
    if (rx == NULL) {
        memset(&dummy_rx, 0, sizeof(dummy_rx));
        rx = &dummy_rx;
    }

    /*
     * Each ACI transmission consists of at least two bytes (packet length
     * and opcode). Each receiving package also has at least two bytes
     * (debug byte and receiving length, which might be zero if there is
     * no actual data available).
     */
    rx->debug  = SPI_master_transmit(tx->length);
    rx->length = SPI_master_transmit(tx->command);
	
    /* Send and receive data while there is data is to send or receive */
    for (i = 0; i < tx->length - 1 || i < rx->length; i++) {
        rx->data[i] = SPI_master_transmit(tx->data[i]);
    }

    reqn_set_high();
    while (rdyn_is_low()) {
        /* wait */
    }

    /*
     * Make sure REQN inactive time (Tcwh, nRF datasheet page 26) is given.
     * Experienced some timing issues, i.e. empty events read after requesting
     * data (simplest case reading temperature) without a small delay here.
     */
    _delay_ms(1);

    return 1;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool nrf_get_evt(struct nrf_rx *rx) {
	nrf_receive(rx);
	if(rx->length > 0) {
		return true;
	} 
	return false;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_print_event(struct nrf_rx *rx) {
	
	dp(" ->evt:");
	print_event_name(rx->data[0]);
	dpv(" len=", rx->length);
	
	switch (rx->data[0]) {
		
		case NRF_EVT_DEVICE_STARTED:
			UARTPrintValDec_P(PSTR("  OpMode = "), rx->data[1]);
			UARTPrintValDec_P(PSTR("  HWError = "), rx->data[2]);
			UARTPrintValDec_P(PSTR("  DataCreditsAvl = "), rx->data[3]);			
		break;
		
		case NRF_EVT_CMD_RESPONSE:
			UARTPrint_P(PSTR(" OpCode = 0x"));
			UARTPrintUint_M(rx->data[1], 16, 2);
			UARTPrint_P(PSTR("  Status = "));
			if (rx->data[2] == 0) {
				UARTPrint("Ok");
				} else {
				UARTPrintUint_M(rx->data[2], 16, 2);
			}
	
			UARTPrint_P(PSTR("  RespData = "));
			if(rx->length > 3) {
				for (uint8_t i = 3; i < rx->length; i++) {
					UARTPrintUint_M(rx->data[i], 16, 2);
					UARTPutChar(' ');
				}
			} else {
				UARTPrintln_P(PSTR("None"));
			}
	
			break;
		
		case NRF_EVT_CONNECTED:
			UARTPrint_P(PSTR(" AddressType = 0x"));
			UARTPrintUint_M(rx->data[1], 16, 2);

			UARTPrint_P(PSTR(" PeerAddress = "));
			for (uint8_t i = 0; i < 5; i++) {
				UARTPrintUint(rx->data[7 - i], 16);
				UARTPutChar(':');
			}
			UARTPrint_P(PSTR(" ConnectionInterval = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[9] << 8) | rx->data[8]), 10);
			UARTPrint_P(PSTR("*1.25 ms"));

			UARTPrint_P(PSTR(" SlaveLatency = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[11] << 8) | rx->data[10]), 10);
	
			UARTPrint_P(PSTR(" SupervisionTimeout = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[13] << 8) | rx->data[12]), 10);
			UARTPrint_P(PSTR("*10 ms"));
	
			UARTPrint_P(PSTR(" MasterClockAccuracy = "));
			UARTPrintUint(rx->data[14], 16);

			break;
		
		case NRF_EVT_TIMING:
			UARTPrint_P(PSTR(" ConnectionInterval = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[1] << 8) | rx->data[2]), 10);
			UARTPrint_P(PSTR("*1.25 ms"));

			UARTPrint_P(PSTR(" SlaveLatency = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[4] << 8) | rx->data[3]), 10);
			
			UARTPrint_P(PSTR(" SupervisionTimeout = "));
			UARTPrintInt16((uint16_t)((uint16_t)(rx->data[6] << 8) | rx->data[5]), 10);
			UARTPrint_P(PSTR("*10 ms"));			

			break;
		
		case NRF_EVT_DISCONNECTED:
			UARTPrint_P(PSTR(" AciStatus = 0x"));
			UARTPrintUint_M(rx->data[1], 16, 2);

			UARTPrint_P(PSTR(" BtLeStatus = 0x"));
			UARTPrintUint_M(rx->data[2], 16, 2);

			break;
		
		case NRF_EVT_BOND_STATUS:
			UARTPrint_P(PSTR(" BondStatusCode = 0x"));
			UARTPrintUint_M(rx->data[1], 16, 2);

			UARTPrint_P(PSTR(" BondStatusSource = 0x"));
			UARTPrintUint_M(rx->data[2], 16, 2);
		
			UARTPrint_P(PSTR(" BondStatus-SecMode1 = 0x"));
			UARTPrintUint_M(rx->data[3], 16, 2);

			UARTPrint_P(PSTR(" BondStatus-SecMode2 = 0x"));
			UARTPrintUint_M(rx->data[4], 16, 2);

			UARTPrint_P(PSTR(" BondStatus-KeyExchSlave = 0x"));
			UARTPrintUint_M(rx->data[5], 16, 2);
		
			UARTPrint_P(PSTR(" BondStatus-KeyExchMaster = 0x"));
			UARTPrintUint_M(rx->data[6], 16, 2);
		
		break;
		
		default:
			UARTPrint_P(PSTR("  print details undefined"));
			break;
	}
	UARTPrintln("");

}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool restore_bond_data() {

	uint8_t st = eeprom_read_byte(EEPROM_DD_STATUS_ADDRESS);				// check if bonded info is stored in eeprom
	
	if ((st ^ EEPROM_OK_STATUS_MASK) >= EEPROM_OK_STATUS_MASK ) {			// no or corrupted bond info in eeprom 
		dpl_p(PSTR("no bond data"));
		return false;
	}
	
	dpl_p(PSTR("bond data found"));
	
	return nrf_restore_dynamic_data(st);									// get bond data from eeprom and write it to nrf8001
	
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_clear_bond_data() {
	eeprom_write_byte(0, 0x00);
	is_bonded = false;
	is_bond_info_stored = false;
	dpl_p(PSTR("bond info deleted (pairing reset)"));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// handler of aci system events 
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_system_event_handler(struct nrf_rx *rx) {
	
    if (!rx->length || (rx->data[0] > 0x89 && rx->data[0] < 0x8E )) {			// exit if no events in rx buffer or there is data event
	    return;
    }
	
	dp("  system");
	
	switch(rx->data[0]) {
		
		case NRF_EVT_DEVICE_STARTED:											// @todo: think about global fatal error flag
		
			if (rx->data[2] != NRF_ERR_NO_ERROR) {
				_delay_ms(20);													// magic number 
			}
			
			switch (rx->data[1]) {												// operating mode
				case NRF_OPMODE_TEST:												
					// ?
				break;
				
				case NRF_OPMODE_SETUP:
					nrf_setup();
				break;
				
				case NRF_OPMODE_STANDBY:
					nrf_data_credits = rx->data[3];														// get number of data credits
					dp_new_line();
					dpl_p(PSTR("ble is up and running"));
					
					if(restore_bond_data()) { 
						is_bond_info_stored = true;
						dp_new_line();
						dpl_p(PSTR("bond data found and restored"));
						nrf_cmd_connect(0/* infinite*/, 0x0020 /* advertising interval 20ms*/);		// advertise in connect mode						
					} else {
						is_bond_info_stored = false;
						nrf_cmd_bond(180/* in seconds */, 0x0050 /* advertising interval 50ms*/);
					}     
				break;
				
			}
			
			break;
		
        case NRF_EVT_CMD_RESPONSE:		
			cmd_resp_awating = false;										// drop cmd resp awaiting flag			
			break;
		
        case NRF_EVT_CONNECTED:
			led_connect_on();
			break;		
		
		case NRF_EVT_TIMING:
// 			if(is_bonded && !is_bond_info_stored) {
// 				nrf_cmd_disconnect(ACI_REASON_TERMINATE);
// 			}
			break;
		
        case NRF_EVT_DISCONNECTED:
			led_connect_off();
			nrf_close_tx_pipes();
		
			if(rx->data[1] == ACI_STATUS_ERROR_ADVT_TIMEOUT) {										// advertising timeout reached
				dpl_p(PSTR("bond timeout reached"));				
			} else {
				if(!is_bonded) {																	// if ble is not bonded
					nrf_cmd_bond(180/* in seconds */, 0x0050 /* advertising interval 50ms*/);			// advertise in bond mode		
					dpl_p(PSTR("waiting to be bonded"));
					dp_new_line();					
				} else {																			// if ble is bonded
					if(!is_bond_info_stored) {														// if bonded first time 
					 	if(nrf_store_bond_data()) {													// store bond data
	 						eeprom_test_read();
							dp_new_line();
							dpl("bond data stored");
							is_bond_info_stored = true;
						}
					}
					nrf_cmd_connect(0/* infinite */, 0x0020 /* advertising interval 20ms*/);		// advertise in connect mode
					dpl_p(PSTR("waiting to be connected"));					
				}
			}
			break;
			
		case NRF_EVT_BOND_STATUS:
			if(rx->data[1] == ACI_BOND_STATUS_SUCCESS) {
				dpl_p(PSTR("bond success"));
				is_bonded = true;
			} else {
				dp("bond error. code: 0x");
				UARTPrintUint_M(rx->data[1], 16, 2);
				dp_new_line();				
				is_bonded = false;
			}
			break;

		case NRF_EVT_PIPE_STATUS:
			/* Assemble pipes_open information from received pipe status */
			pipes_open = 0;
			for (uint8_t i = 0; i < 8; i++) {
				pipes_open |= rx->data[i+1] << (8 * i);
			}

			#ifdef DEBUG_P
			UARTPrint_P(PSTR("open pipes:"));
			for (uint8_t i = 1; i <= NUMBER_OF_PIPES; i++) {
				if (pipes_open & (1 << i)) {
					UARTPrintUint(i, 10);
					UARTPutChar(' ');
				}
			}
			UARTPrintln("");
			#endif
			
			break;
/*
		case NRF_EVT_DATA_RECEIVED:
//			if (rx->data[1] == PIPE_EXAMPLE_SERVICE_PWM_DUTY_CYCLE_RX) {
			if (rx->data[1] == PIPE_SI_ZONE_SERVICE_0_ZONE_CONFIG_RX) {
				if (rx->data[2] > 0) {
					OCR0A = rx->data[2];
					TCCR0A = 0x83; // Fast PWM (mode 4), clear on match and set on bottom
					TCCR0B = 0x04; // Fast PWM (mode 4), prescaler 256
				} else {
					TCCR0A = 0x00;
					TCCR0B = 0x00;
					PORTD &= ~(1 << PD6);
				}
			}
			break;
*/
		default:
			break;	
	}
	
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// handler of aci data events
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void nrf_data_event_handler(struct nrf_rx *rx) {
	
	if (!rx->length || !(rx->data[0] > 0x89 && rx->data[0] < 0x8E )) {			// exit if no events in rx buffer or there is system event
		return;
	}
	
	dp("  data");
	switch(rx->data[0]) {
		case NRF_EVT_DATA_RECEIVED:		
//			if (rx->data[1] == PIPE_EXAMPLE_SERVICE_PWM_DUTY_CYCLE_RX) {
			if (rx->data[1] == PIPE_CURRENT_TIME_CURRENT_TIME_RX_ACK_AUTO) {
				
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
				
				rtc_time l_rt;
				
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
				

				nrf_print_rx(rx);
/*				
				if (rx->data[2] > 0) {
					OCR0A = rx->data[2];
					TCCR0A = 0x83; // Fast PWM (mode 4), clear on match and set on bottom
					TCCR0B = 0x04; // Fast PWM (mode 4), prescaler 256
				} else {
					TCCR0A = 0x00;
					TCCR0B = 0x00;
					PORTD &= ~(1 << PD6);
				}
*/				
			}
			break;

		default:
			break;
	}
}


//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void print_event_name(uint8_t evt_code) {
	
	const char *str;
	
	switch(evt_code) {
		case 0x81:
			str = PSTR("DeviceStarted");
		break;
		case 0x82:
			str = PSTR("Echo");		
		break;
		case 0x83:
			str = PSTR("HardwareError");		
		break;
		case 0x84:
			str = PSTR("CommandResponse");
		break;
		case 0x85:
			str = PSTR("Connected");
		break;
		case 0x86:
			str = PSTR("Disconnected");
		break;
		case 0x87:
			str = PSTR("BondStatus");
		break;
		case 0x88:
			str = PSTR("PipeStatus");
		break;
		case 0x89:
			str = PSTR("Timing");
		break;
		case 0x8E:
			str = PSTR("DisplayKey");
		break;
		case 0x8F:
			str = PSTR("KeyRequest");
		break;
		
		//data
		case 0x8A:
			str = PSTR("DataCredit");
		break;
		case 0x8D:
			str = PSTR("PipeError");
		break;
		case 0x8C:
			str = PSTR("DataReceived");
		break;
		case 0x8B:
			str = PSTR("DataAck");
		break;
		
		default:
			str = PSTR("_UNKNOWN_");
		break;
	}
	
	UARTPrint_P(str);
	UARTPrint_P(PSTR("Event"));
}


/**
 * Send the given button state to the remote side.
 *
 * If the button state tx pipe is not open (i.e. remote side is not waiting
 * for notifications about the new state), nothing will happen.
 *
 * @param button Button state to send
 * @return 0 on success, -1 if tx pipe is closed
 */
int8_t
nrf_send_button_data(uint8_t button)
{ 
/*	
//    if (!(pipes_open & (1 << PIPE_EXAMPLE_SERVICE_BUTTON_STATE_TX))) {
    if (!(pipes_open & (1 << PIPE_SI_CONTROLLER_SERVICE_CONTROLLER_CONFIG_RD_TX))) {	
        dpl_p(PSTR("Pipe not open"));
        return -1;
    }

    memset(&tx, 0, sizeof(tx));

    tx.length = 3;
    tx.command = NRF_CMD_SEND_DATA;
//    tx.data[0] = PIPE_EXAMPLE_SERVICE_BUTTON_STATE_TX;
    tx.data[0] = PIPE_SI_CONTROLLER_SERVICE_CONTROLLER_CONFIG_RD_TX;
    tx.data[1] = button;

    nrf_send(&tx);
*/
    return 0;
}

/**
 * Print the given rx struct content.
 *
 * Mainly meant for debug
 *
 * @param rx Received data to print
 * @return none
 */
void nrf_print_rx(struct nrf_rx *rx) {
    uint8_t i;

    UARTPutChar('[');
    UARTPrintUint_M(rx->length, 10, 2);
    UARTPutChar(']');
    
    for (i = 0; i < rx->length; i++) {
        UARTPutChar(' ');
        UARTPrintUint_M(rx->data[i], 16, 2);
    }
    UARTPrintln("");
}

/**
 * Print the nRF8001 module's on-chip temperature.
 *
 * Send temperature command to the module and receive it,
 * then output it via UART.
 *
 * @param none
 * @return none
 */
void
nrf_print_temperature(void)
{
    data16_t raw;

    memset(&tx, 0, sizeof(tx));
    memset(&rx, 0, sizeof(rx));

    tx.length = 0x01;
    tx.command = NRF_CMD_GET_TEMPERATURE;
    nrf_txrx(&tx, &rx);

    raw.lsb = rx.data[3];
    raw.msb = rx.data[4];

    UARTPrint_P(string_temperature);
    UARTPrintInt16(raw.word >> 2, 10);
    UARTPutChar('.');
    switch (raw.word & 0x03) {
        case 0:
            UARTPutChar('0');
            UARTPutChar('0');
        break;
        case 1:
            UARTPutChar('2');
            UARTPutChar('5');
        break;
        case 2:
            UARTPutChar('5');
            UARTPutChar('0');
        break;
        case 3:
            UARTPutChar('7');
            UARTPutChar('5');
        break;
    }

    UARTPrint_P(string_celsius);
}


void nrf_cmd_read_dynamic_data() {

    memset(&tx, 0, sizeof(tx));

    tx.length = MSG_READ_DYNAMIC_DATA_LEN;
    tx.command = NRF_CMD_READ_DYNAMIC_DATA;

    dpl_p(PSTR("CMD->: ReadDynamicData"));
    nrf_send(&tx);

}

void nrf_cmd_disconnect(uint8_t reason) {

	memset(&tx, 0, sizeof(tx));

	tx.length = MSG_DISCONNECT_LEN;
	tx.command = NRF_CMD_DISCONNECT;
	
	tx.data[0] = reason;

	dpl_p(PSTR("CMD->: Disconnect reason: 0x"));
	UARTPrintUint_M(reason, 16, 2);
	dp_new_line();
	nrf_send(&tx);

}

void nrf_cmd_radio_reset() {

	memset(&tx, 0, sizeof(tx));

	tx.length = MSG_RADIO_RESET_LEN;
	tx.command = NRF_CMD_RADIO_RESET;
	
	dpl_p(PSTR("CMD->: Radio Reset"));
	nrf_send(&tx);

}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void eeprom_test_read() {
	
	uint8_t byte;
	UARTPrintln("");
	UARTPrintln("   eeprom test read:");
	
	for (uint16_t i = 0; i <150; i++) {
		byte = eeprom_read_byte((const uint8_t*) i);
		UARTPrintUint_M(byte, 16, 2);
		UARTPutChar(' ');
	}
	
	
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// restore dynamic data from eeprom to nrf8001
bool nrf_restore_dynamic_data(uint8_t eeprom_status_byte) {
	
	uint8_t  dd_msg_num = eeprom_status_byte & ~EEPROM_OK_STATUS_MASK;			// get dd msg num
	uint16_t read_offset = EEPROM_DD_START_OFFSET;								// set initial read offset value
	uint8_t  msg_len;															// nrf dd message length
	
	
	dp_new_line(); 	
	dpui(eeprom_status_byte, 10);
	dp_new_line();
	dplv("msg num: ", dd_msg_num ); 	
	dp_new_line();
	
	while(dd_msg_num--) {														// while we have messages 
	
		/* reading dd from eeprom*/
		msg_len = eeprom_read_byte((const uint8_t*) read_offset++);				// reading message length
		
		tx.length = msg_len;													// set tx header
		tx.command = NRF_CMD_WRITE_DYNAMIC_DATA;								// set tx header			
		++read_offset;															// skip command code stored in eeprom
	
		eeprom_read_block(tx.data, (uint16_t*)read_offset, msg_len - 1);		// read msg body from eeprom
		read_offset += msg_len - 1;												// shift offset 
		
		#ifdef DEBUG_P
		dp("-->>TX FROM EEPROM");		
		for (uint8_t i = 0; i < msg_len + 1; i++) {
			UARTPrintUint_M((uint8_t) *((uint8_t*)&tx + i), 16, 2);
			dp(" ");
		}
		dpl("<<--TX FROM EEPROM");
		#endif
		
		/* write dd to nrf */
		nrf_send(&tx);																					// sending tx to nrf8001
		dp("WDD msg sent.");
		while(!nrf_get_evt(&rx));																		// waiting for feedback
		dp(" got resp...status = 0x");
		UARTPrintUint_M(rx.data[2], 16, 2);
		
		if(rx.data[0] != NRF_EVT_CMD_RESPONSE || rx.data[1] != NRF_CMD_WRITE_DYNAMIC_DATA) {			// check if CmdRespEvt has been received with correct opt code
			dpl(" RDD: wrong resp msg type");
			return false;
		}
		
		if (!dd_msg_num) {																				// if last dd message has been sent
			if(rx.data[2] != ACI_STATUS_TRANSACTION_COMPLETE) {											// must be transaction complete status
				dp(" RDD ERROR: no msg left but dd is not complete");
				return false;
			}
		} else {																						// if it's not last message 
			if(rx.data[2] != ACI_STATUS_TRANSACTION_CONTINUE) {											// must be transaction continue status
				dp(" RDD ERROR: wrong resp status");
				return false;
			}
		}
		dpl(" ok");
	}
	
	return true;
	
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// stores in the format of WriteDynamicData nrf8001 system command
void nrf_store_dynamic_data_to_eeprom(struct nrf_rx* rx) {
	
	static int eeprom_write_offset = EEPROM_DD_START_OFFSET;
	
	dp_p(PSTR("--WDD SAVE-->"));
	dpui(rx->length-2, 10);  
	dp(" ");
	
	eeprom_write_byte((uint8_t*) eeprom_write_offset++, rx->length-2);
	
	UARTPrintUint_M(NRF_CMD_WRITE_DYNAMIC_DATA, 16, 2);  UARTPutChar(' ');
	eeprom_write_byte((uint8_t*) eeprom_write_offset++, NRF_CMD_WRITE_DYNAMIC_DATA);
		
	for (uint8_t i = 3; i < rx->length; i++ ) {
		UARTPrintUint_M(rx->data[i], 16, 2);  UARTPutChar(' ');
		eeprom_write_byte((uint8_t*) eeprom_write_offset++, rx->data[i]);
	}
	
	dpl_p(PSTR("<--WDD SAVE--"));
}
    
/* Read Dynamic Data 
*
*
*
*/
bool nrf_store_bond_data() {
	
	uint8_t read_dyn_num_msgs = 0;
  
	//Start reading the dynamic data
	nrf_cmd_read_dynamic_data();
	read_dyn_num_msgs++;	

	while(1) {
		if (nrf_get_evt(&rx)) {
			
			nrf_print_rx(&rx);
			
			if(rx.data[1] != NRF_CMD_READ_DYNAMIC_DATA) {													// Got something other than a command response evt -> Error  @todo - put to queue .... ?
				eeprom_write_byte(EEPROM_DD_STATUS_ADDRESS, EEPROM_ERROR_STATUS);							// put error status in eeprom 
				dpl_p(PSTR("RDD Error1"));
				return false;
			}
			
			if (rx.data[2] == ACI_STATUS_TRANSACTION_COMPLETE) {											// got last message 
				nrf_store_dynamic_data_to_eeprom(&rx);
				eeprom_write_byte(EEPROM_DD_STATUS_ADDRESS, EEPROM_OK_STATUS_MASK | read_dyn_num_msgs );
				
				dpl_p(PSTR("RDD Complete"));
				return true;
			}

			if (rx.data[2] != ACI_STATUS_TRANSACTION_CONTINUE) {											// got not transaction continue but something else 
				eeprom_write_byte(EEPROM_DD_STATUS_ADDRESS, EEPROM_ERROR_STATUS);
				dpl_p(PSTR("RDD Error2 "));					
				return false;			
			} else {
				dpl_p(PSTR("RDD Continue"));														// transaction continue - reading next message
				nrf_store_dynamic_data_to_eeprom(&rx);
				nrf_cmd_read_dynamic_data();
				read_dyn_num_msgs++;
			}
			
		}
	}
	
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void aci_loop() {
	nrf_receive(&rx);
		if (rx.length > 0) {
			nrf_print_rx(&rx);

			event_dp(&rx);	
	        
	        nrf_system_event_handler(&rx);
//	        nrf_data_event_handler(&rx);
			
			nrf_extern_system_event_handler(&rx);
			nrf_extern_data_event_handler(&rx);

	        memset(&rx, 0, sizeof(rx));
        } else {
	        _delay_ms(20);
        }	
}

