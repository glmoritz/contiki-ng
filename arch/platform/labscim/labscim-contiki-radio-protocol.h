/*
 * labscim-contiki-radio-protocol.h
 *
 *  Created on: 9 de jun de 2020
 *      Author: root
 */

#ifndef DEV_LABSCIM_CONTIKI_RADIO_PROTOCOL_H_
#define DEV_LABSCIM_CONTIKI_RADIO_PROTOCOL_H_


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define CONTIKI_RADIO_SETUP (0x1111)
/**
 * This is setup command for the configuration of the omnet radio
 */
struct contiki_radio_setup
{
	uint32_t Power_dbm;
	uint32_t Bitrate_bps;
	uint32_t Frequency_Hz;
	uint32_t Bandwidth_Hz;
} __attribute__((packed));


#define CONTIKI_RADIO_SET_MODE (0x2222)
typedef enum {
	/**
	 * The radio is turned off, frame reception or transmission is not
	 * possible, power consumption is zero, radio mode switching is slow.
	 */
	RADIO_MODE_OFF,

	/**
	 * The radio is sleeping, frame reception or transmission is not possible,
	 * power consumption is minimal, radio mode switching is fast.
	 */
	RADIO_MODE_SLEEP,

	/**
	 * The radio is prepared for frame reception, frame transmission is not
	 * possible, power consumption is low when receiver is idle and medium
	 * when receiving.
	 */
	RADIO_MODE_RECEIVER,

	/**
	 * The radio is prepared for frame transmission, frame reception is not
	 * possible, power consumption is low when transmitter is idle and high
	 * when transmitting.
	 */
	RADIO_MODE_TRANSMITTER,

	/**
	 * The radio is prepared for simultaneous frame reception and transmission,
	 * power consumption is low when transceiver is idle, medium when receiving
	 * and high when transmitting.
	 */
	RADIO_MODE_TRANSCEIVER,

	/**
	 * The radio is switching from one mode to another, frame reception or
	 * transmission is not possible, power consumption is minimal.
	 */
	RADIO_MODE_SWITCHING    // this radio mode must be the very last
} RadioMode;

/**
 * This sets the radio in the different operation modes
 */

struct contiki_radio_mode
{
	uint8_t RadioMode;
} __attribute__((packed));


#define CONTIKI_RADIO_SEND (0x3333)
#define CONTIKI_RADIO_SEND_COMPLETED (0x3334)
#define CONTIKI_RADIO_PACKET_RECEIVED (0x4444)
/**
 * This is a sent/received radio payload
 */
struct contiki_radio_payload
{
	uint16_t MessageSize_bytes;
	int32_t RSSI_dbm_x100;
	uint32_t LQI;
	uint64_t RX_timestamp_us;
	uint8_t Message[];
} __attribute__((packed));
#define FIXED_SIZEOF_CONTIKI_RADIO_PAYLOAD (sizeof(uint16_t) + 2*sizeof(uint32_t) + sizeof(uint64_t))

struct contiki_radio_send_response
{
	uint32_t ResponseCode;
} __attribute__((packed));



#define CONTIKI_RADIO_PERFORM_CCA (0x5555) //for this message, the payload is ignored (but must be sent)
#define CONTIKI_RADIO_CCA_RESULT (0x6666)

struct contiki_radio_cca
{
	uint32_t ChannelIsFree;
} __attribute__((packed));


#define CONTIKI_RADIO_GET_STATE (0x7777)
#define CONTIKI_RADIO_STATE_RESULT (0x8888)
#define CONTIKI_RADIO_STATE_CHANGED (0x8889)
//state may be used to perform CCA as well, but CONTIKI_RADIO_PERFORM_CCA simulates the CCA time while get state is instantaneous

//same macro from omnet
enum ReceptionState {
       /**
        * The radio medium state is unknown, reception state is meaningless,
        * signal detection is not possible. (e.g. the radio mode is off, sleep
        * or transmitter)
        */
       RECEPTION_STATE_UNDEFINED,

       /**
        * The radio medium is free, no signal is detected. (e.g. the RSSI is
        * below the energy detection threshold)
        */
       RECEPTION_STATE_IDLE,

       /**
        * The radio medium is busy, a signal is detected but it is not strong
        * enough to receive. (e.g. the RSSI is above the energy detection
        * threshold but below the reception threshold)
        */
       RECEPTION_STATE_BUSY,

       /**
        * The radio medium is busy, a signal strong enough to receive is detected.
        * (e.g. the SNIR was above the reception threshold)
        */
       RECEPTION_STATE_RECEIVING
   };

struct contiki_radio_state
{
	uint32_t State;
} __attribute__((packed));




#endif /* DEV_LABSCIM_CONTIKI_RADIO_PROTOCOL_H_ */
