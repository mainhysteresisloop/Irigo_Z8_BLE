/*
 * nRF8001 handling code
 * Part of the Bluetooth LE example system
 *
 * Copyright 2017 Sven Gregori
 * Released under MIT License
 *
 */
#ifndef _NRF_H_
#define _NRF_H_

#include "services.h"
#include <stdbool.h>

#define BLE_REQN_DDR   DDRB
#define BLE_REQN_PORT  PORTB
#define BLE_REQN_PIN   PB1

#define BLE_RDYN_DDR   DDRD
#define BLE_RDYN_PORT  PORTD
#define BLE_RDYN_PIN   PD3
#define BLE_RDYN_PP    PIND     // PP - Pin Port

#define BLE_RESET_DDR  DDRB
#define BLE_RESET_PORT PORTB
#define BLE_RESET_PIN  PB0

#define MSG_SET_LOCAL_DATA_BASE_LEN              2
#define MSG_CONNECT_LEN                          5
#define MSG_BOND_LEN                             5
#define MSG_READ_DYNAMIC_DATA_LEN                1
#define MSG_DISCONNECT_LEN                       2
#define MSG_BASEBAND_RESET_LEN                   1
#define MSG_WAKEUP_LEN                           1
#define MSG_SET_RADIO_TX_POWER_LEN               2
#define MSG_RADIO_RESET_LEN						 1
#define MSG_GET_DEVICE_ADDR_LEN                  1
#define MSG_SEND_DATA_BASE_LEN                   2
#define MSG_DATA_REQUEST_LEN                     2
#define MSG_OPEN_REMOTE_PIPE_LEN                 2
#define MSG_CLOSE_REMOTE_PIPE_LEN                2
#define MSG_DTM_CMD                              3
#define MSG_WRITE_DYNAMIC_DATA_BASE_LEN          2
#define MSG_SETUP_CMD_BASE_LEN                   1
#define MSG_ECHO_MSG_CMD_BASE_LEN                1
#define MSG_CHANGE_TIMING_LEN                    9
#define MSG_SET_APP_LATENCY_LEN                  4
#define MSG_CHANGE_TIMING_LEN_GAP_PPCP           1
#define MSG_DIRECT_CONNECT_LEN                   1
#define MSG_SET_KEY_REJECT_LEN                   2
#define MSG_SET_KEY_PASSKEY_LEN                  8
#define MSG_SET_KEY_OOB_LEN                      18
#define MSG_ACK_LEN                              2
#define MSG_NACK_LEN                             3
#define MSG_BROADCAST_LEN                        5
#define MSG_OPEN_ADV_PIPES_LEN                   9

#define ACI_STATUS_SUCCESS						0x00 
#define ACI_STATUS_TRANSACTION_CONTINUE			0x01
#define ACI_STATUS_TRANSACTION_COMPLETE			0x02

#define NRF_OPMODE_TEST     0x01
#define NRF_OPMODE_SETUP    0x02
#define NRF_OPMODE_STANDBY  0x03

#define NRF_STATE_DISCONNECT  0x00
#define NRF_STATE_CONNECTING  0x01
#define NRF_STATE_CONNECTED   0x02

#define NRF_STATE_CMD_NONE        0x00
#define NRF_STATE_CMD_ADV_NC      0x01
#define NRF_STATE_CMD_BOND        0x02
#define NRF_STATE_CMD_DISCONNECT  0x03
#define NRF_STATE_CMD_CONNECT     0x04
#define NRF_STATE_CMD_RADIO_RESET 0x05

#define NRF_CMD_BROADCAST           0x1C
#define NRF_CMD_SETUP               0x06
#define NRF_CMD_READ_DYNAMIC_DATA   0x07
#define NRF_CMD_WRITE_DYNAMIC_DATA  0x08
#define NRF_CMD_GET_TEMPERATURE		0x0c
#define NRF_CMD_RADIO_RESET			0x0e
#define NRF_CMD_CONNECT				0x0f
#define NRF_CMD_BOND				0x10
#define NRF_CMD_DISCONNECT			0x11
#define NRF_CMD_SEND_DATA			0x15
#define NRF_ERR_NO_ERROR			0x00
#define NRF_EVT_DEVICE_STARTED		0x81
#define NRF_EVT_CMD_RESPONSE		0x84
#define NRF_EVT_CONNECTED			0x85
#define NRF_EVT_DISCONNECTED		0x86
#define NRF_EVT_BOND_STATUS			0x87
#define NRF_EVT_PIPE_STATUS			0x88
#define NRF_EVT_TIMING				0x89
#define NRF_EVT_DATA_RECEIVED		0x8c


#define DEBUG_P

#ifdef DEBUG_P
#define event_dp(x)		 nrf_print_event(x)
#define dp(x)			 UARTPrint(x)
#define dp_p(x)			 UARTPrint_P(x)
#define dpl(x)			 UARTPrintln(x)
#define dpl_p(x)		 UARTPrintln_P(x)
#define dp_new_line()	 UARTPrintln("")
#define dplv(name, val)	 UARTPrintlnValDec(name, val)
#define dpv(name, val)	 UARTPrintValDec(name, val)
#define dpv_p(name, val) UARTPrintValDec_P(name, val)
#define dpui(ui, raddix) UARTPrintUint(ui, raddix);
#else
#define event_dp(x)
#define dp(x)
#define dp_p(x)
#define dpl(x)
#define dpl_p(x)
#define dp_new_line()
#define dplv(name, val)
#define dpv(name, val)
#define dpv_p(name, val)
#define dpui(ui, raddix)
#endif


typedef union {
    uint8_t byte[2]; /* byte[0] = lsb, byte[1] = msb */
    uint16_t word;
    int16_t s_word;
    struct {
        uint8_t lsb;
        uint8_t msb;
    };
} data16_t;

struct nrf_setup_data {
    uint8_t status;
    uint8_t data[32];
};

struct nrf_tx {
    uint8_t length;
    uint8_t command;
    uint8_t data[30];
};

struct nrf_rx {
    uint8_t debug;
    uint8_t length;
    uint8_t data[30];
};

#define DATA_CREDITS_MAX 2

bool tx_to_send;				// there is message to send in tx buffer
bool rx_received;				// there is received message in rx buffer
bool cmd_resp_awating;			// nrf8001 got the command, MCU must wait for CommandResponseEvent before pushing next command

uint8_t data_credits_left;

void aci_loop();

void nrf_system_event_handler(struct nrf_rx *rx);
void nrf_data_event_handler();
// void nrf_cmd_processor();
// void nrf_data_processor();

void nrf_pins_setup();
void nrf_reset_module(void);
int8_t nrf_setup(void);
void nrf_start();

void nrf_cmd_connect(uint16_t run_timeout, uint16_t adv_interval);
void nrf_cmd_bond(uint16_t run_timeout, uint16_t adv_interval);
void nrf_cmd_broadcast(void);
void nrf_cmd_radio_reset();
void nrf_cmd_disconnect(uint8_t reason) ;

int8_t nrf_transmit(struct nrf_tx *tx, struct nrf_rx *rx);
#define nrf_send(tx) nrf_transmit(tx, NULL)
#define nrf_receive(rx) nrf_transmit(NULL, rx)
#define nrf_txrx(tx, rx) do { nrf_transmit(tx, NULL); nrf_transmit(NULL, rx); } while (0);
bool nrf_get_evt(struct nrf_rx *rx);

int8_t nrf_send_button_data(uint8_t button);
void nrf_print_rx(struct nrf_rx *rx);
void nrf_print_temperature(void);

bool nrf_read_dynamic_data();
void nrf_clear_bond_data();

//extern struct nrf_rx rx;

#define led_setup_on()      do { PORTD |= 0x80; } while (0)
#define led_setup_off()     do { PORTD &= ~(0x80); } while (0)
#define led_connect_on()    do { PORTD |= 0x10; } while (0)
#define led_connect_off()   do { PORTD &= ~(0x10); } while (0)

#define ble_reset_high()    BLE_RESET_PORT |=  _BV(BLE_RESET_PIN)
#define ble_reset_low()     BLE_RESET_PORT &= ~_BV(BLE_RESET_PIN)
#define reqn_set_high()     BLE_REQN_PORT |=   _BV(BLE_REQN_PIN)
#define reqn_set_low()      BLE_REQN_PORT &=  ~_BV(BLE_REQN_PIN) 
#define rdyn_is_high()      (BLE_RDYN_PP & _BV(BLE_RDYN_PIN))
#define rdyn_is_low()       (!(BLE_RDYN_PP & _BV(BLE_RDYN_PIN)))

struct service_pipe_mapping {
    aci_pipe_store_t store;
    aci_pipe_type_t type;
};

void nrf_tx_map_pipes(void);

#endif /* _NRF_H_ */
