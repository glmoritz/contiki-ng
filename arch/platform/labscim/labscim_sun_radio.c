/*
 * This is the Labscim Radio driver, which is based on the cooja radio driver and cc13xx prop mode driver
 *
 *  Author: Guilherme Luiz Moritz <moritz@utfpr.edu.br>
 *
 *
 * Below we have the unmodified cooja radio driver disclamer notice and the unmodified texas instruments disclamer notice:
 *
 *
 *
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/*
 * Copyright (c) 2015, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/


#include "labscim_sun_radio.h"

#include <stdio.h>
#include <string.h>

#include "contiki.h"

#include "net/packetbuf.h"
#include "net/netstack.h"
#include "sys/energest.h"

#include "dot-15-4g.h"
#include "prop-mode.h"

#include "dev/radio.h"
#include "labscim-contiki-radio-protocol.h"
#include "labscim_socket.h"
#include "labscim_protocol.h"
#include "platform.h"

/*---------------------------------------------------------------------------*/
/* TX power table for the 431-527MHz band */
#ifdef PROP_MODE_CONF_TX_POWER_431_527
#define PROP_MODE_TX_POWER_431_527 PROP_MODE_CONF_TX_POWER_431_527
#else
#define PROP_MODE_TX_POWER_431_527 prop_mode_tx_power_431_527
#endif
/*---------------------------------------------------------------------------*/
/* TX power table for the 779-930MHz band */
#ifdef PROP_MODE_CONF_TX_POWER_779_930
#define PROP_MODE_TX_POWER_779_930 PROP_MODE_CONF_TX_POWER_779_930
#else
#define PROP_MODE_TX_POWER_779_930 prop_mode_tx_power_779_930
#endif
/*---------------------------------------------------------------------------*/
/* Select power table based on the frequency band */
#if DOT_15_4G_FREQUENCY_BAND_ID==DOT_15_4G_FREQUENCY_BAND_470
#define TX_POWER_DRIVER PROP_MODE_TX_POWER_431_527
#else
#define TX_POWER_DRIVER PROP_MODE_TX_POWER_779_930
#endif
/*---------------------------------------------------------------------------*/
extern const prop_mode_tx_power_config_t TX_POWER_DRIVER[];

/* Max and Min Output Power in dBm */
#define OUTPUT_POWER_MAX     (TX_POWER_DRIVER[0].dbm)
#define OUTPUT_POWER_UNKNOWN 0xFFFF

/* Default TX Power - position in output_power[] */
static const prop_mode_tx_power_config_t *tx_power_current = &TX_POWER_DRIVER[0];
/*---------------------------------------------------------------------------*/
#ifdef PROP_MODE_CONF_LO_DIVIDER
#define PROP_MODE_LO_DIVIDER   PROP_MODE_CONF_LO_DIVIDER
#else
#define PROP_MODE_LO_DIVIDER   0x05
#endif
/*---------------------------------------------------------------------------*/
#ifdef PROP_MODE_CONF_RX_BUF_CNT
#define PROP_MODE_RX_BUF_CNT PROP_MODE_CONF_RX_BUF_CNT
#else
#define PROP_MODE_RX_BUF_CNT 4
#endif
/*---------------------------------------------------------------------------*/
#define DATA_ENTRY_LENSZ_NONE 0
#define DATA_ENTRY_LENSZ_BYTE 1
#define DATA_ENTRY_LENSZ_WORD 2 /* 2 bytes */

/* The size of the metadata (excluding the packet length field) */
#define RX_BUF_METADATA_SIZE \
  (CRC_LEN * RF_CORE_RX_BUF_INCLUDE_CRC \
      + RF_CORE_RX_BUF_INCLUDE_RSSI \
      + RF_CORE_RX_BUF_INCLUDE_CORR \
      + 4 * RF_CORE_RX_BUF_INCLUDE_TIMESTAMP)

/* The offset of the packet length in a rx buffer */
#define RX_BUF_LENGTH_OFFSET sizeof(rfc_dataEntry_t)
/* The offset of the packet data in a rx buffer */
#define RX_BUF_DATA_OFFSET (RX_BUF_LENGTH_OFFSET + DOT_4G_PHR_LEN)

#define ALIGN_TO_4(size)	(((size) + 3) & ~3)

#define RX_BUF_SIZE ALIGN_TO_4(RX_BUF_DATA_OFFSET          \
      + MAX_PAYLOAD_LEN \
      + RX_BUF_METADATA_SIZE)


/*
 * The maximum number of bytes this driver can accept from the MAC layer for
 * transmission or will deliver to the MAC layer after reception. Includes
 * the MAC header and payload, but not the FCS.
 */
#ifdef LABSCIM_RADIO_CONF_BUFSIZE
#define LABSCIM_RADIO_BUFSIZE LABSCIM_RADIO_CONF_BUFSIZE
#else
#define LABSCIM_RADIO_BUFSIZE 125
#endif

extern buffer_circ_t* gNodeOutputBuffer;
extern void labscim_set_time(uint64_t time);

/* Control Variables */
uint32_t gRadioSetupPending=1;
struct contiki_radio_setup gRadioParameters;
uint32_t gCurrentMode = 0;

char simReceiving = 0;

struct labscim_ll gReceivedPackets;
struct labscim_ll gOutboundPackets;

rtimer_clock_t simLastPacketTimestamp = 0;

char simRadioHWOn = 1;

int simSignalStrength = -100;
int simLastSignalStrength = -100;
char simPower = 100;
int simRadioChannel = 26;
int simLQI = 105;
uint32_t simRadioState = 0;

/* If we are in the polling mode, poll_mode is 1; otherwise 0 */
static int poll_mode = 0; /* default 0, disabled */
static int auto_ack = 0; /* AUTO_ACK is not supported; always 0 */
static int addr_filter = 0; /* ADDRESS_FILTER is not supported; always 0 */
static int send_on_cca = (LABSCIM_TRANSMIT_ON_CCA != 0);

PROCESS(labscim_radio_process, "labscim radio process");

//labscim-radio-protocol-functions

void labscim_radio_incoming_command(struct labscim_radio_response* resp)
{
	switch(resp->radio_response_code)
	{
		case CONTIKI_RADIO_PACKET_RECEIVED:
		{
			//just append to the list and wait for processing
			labscim_ll_insert_at_back(&gReceivedPackets, (void*) resp);
			process_poll(&labscim_radio_process);
			break;
		}
		case CONTIKI_RADIO_STATE_CHANGED:
		{
			struct contiki_radio_state* response = (struct contiki_radio_state*)resp->radio_struct;
			simRadioState  = response->State;
			free(resp);
			break;
		}
		default:
		{
			free(resp);
			break;
		}
	}
}


/*---------------------------------------------------------------------------*/
static void
set_send_on_cca(uint8_t enable)
{
	send_on_cca = enable;
}

/*---------------------------------------------------------------------------*/
static void
set_frame_filtering(int enable)
{
	addr_filter = enable;
}

/*---------------------------------------------------------------------------*/
static void
set_auto_ack(int enable)
{
	auto_ack = enable;
}

/*---------------------------------------------------------------------------*/
static void
set_poll_mode(int enable)
{
	poll_mode = enable;
}

/*---------------------------------------------------------------------------*/

int
radio_signal_strength_last(void)
{
	return simLastSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_current(void)
{
	return simSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_LQI(void)
{
	return simLQI;
}

static int radio_set_mode(uint8_t mode)
{
	struct contiki_radio_mode crm;
	crm.RadioMode = mode;
	if(gCurrentMode != mode)
	{
		gCurrentMode = mode;
		radio_command(gNodeOutputBuffer, CONTIKI_RADIO_SET_MODE, (void*)&crm, sizeof(struct contiki_radio_mode));
	}
	return 1;
}


/*---------------------------------------------------------------------------*/
static int
radio_rx_on(void)
{
	ENERGEST_ON(ENERGEST_TYPE_LISTEN);
	radio_set_mode(RADIO_MODE_RECEIVER);
	simRadioHWOn = 1;
	return 1;
}

/*---------------------------------------------------------------------------*/
static int
radio_tx_on(void)
{
	ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
	radio_set_mode(RADIO_MODE_TRANSMITTER);
	simRadioHWOn = 1;
	return 1;
}

/*---------------------------------------------------------------------------*/
static int
radio_rx_off(void)
{
	ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
	radio_set_mode(RADIO_MODE_SLEEP);
	simRadioHWOn = 0;
	return 1;
}

/*---------------------------------------------------------------------------*/
static int
radio_tx_off(void)
{
	ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
	radio_set_mode(RADIO_MODE_SLEEP);
	simRadioHWOn = 0;
	return 1;
}


static uint8_t
get_channel(void)
{
  return (uint8_t)(((float)gRadioParameters.Frequency_Hz -(float)DOT_15_4G_CHAN0_FREQUENCY)/(float)DOT_15_4G_CHANNEL_SPACING);
}
/*---------------------------------------------------------------------------*/
static void
set_channel(uint8_t channel)
{
  uint32_t new_freq;
  new_freq = (DOT_15_4G_CHAN0_FREQUENCY + (channel * DOT_15_4G_CHANNEL_SPACING))*1000;
  if(new_freq != gRadioParameters.Frequency_Hz)
  {
	  gRadioSetupPending = 1;
	  gRadioParameters.Frequency_Hz = new_freq;
  }

}
/*---------------------------------------------------------------------------*/
static uint8_t
get_tx_power_array_last_element(void)
{
  const prop_mode_tx_power_config_t *array = TX_POWER_DRIVER;
  uint8_t count = 0;

  while(array->tx_power != OUTPUT_POWER_UNKNOWN) {
    count++;
    array++;
  }
  return count - 1;
}
/*---------------------------------------------------------------------------*/
/* Returns the current TX power in dBm */
static radio_value_t
get_tx_power(void)
{
  return tx_power_current->dbm;
}


static void
set_tx_power(radio_value_t power)
{
  int i;

  for(i = get_tx_power_array_last_element(); i >= 0; --i) {
    if(power <= TX_POWER_DRIVER[i].dbm) {
      /*
       * Merely save the value. It will be used in all subsequent usages of
       * CMD_PROP_RADIO_DIV_SETP, including one immediately after this function
       * has returned
       */
    	if(gRadioParameters.Power_dbm != (uint32_t)tx_power_current->dbm)
    	{
    		tx_power_current = &TX_POWER_DRIVER[i];
    		gRadioParameters.Power_dbm = (uint32_t)tx_power_current->dbm;
    		gRadioSetupPending = 1;
    	}
      return;
    }
  }
}



/*---------------------------------------------------------------------------*/
static int
radio_read(void *buf, unsigned short bufsize)
{
	struct contiki_radio_payload* payload;
	uint16_t message_size;
	struct labscim_radio_response* msg = labscim_ll_pop_front(&gReceivedPackets);
	payload = (struct contiki_radio_payload*)(msg->radio_struct);

	if(msg == NULL) {
		return 0;
	}
	message_size = payload->MessageSize_bytes;


	if(bufsize < message_size)
	{
		free(msg);
		return 0;
	}
	memcpy(buf, payload->Message, message_size);
	if(!poll_mode) {
		/* Not in poll mode: packetbuf should not be accessed in interrupt context.
		 * In poll mode, the last packet RSSI and link quality can be obtained through
		 * RADIO_PARAM_LAST_RSSI and RADIO_PARAM_LAST_LINK_QUALITY */
		packetbuf_set_attr(PACKETBUF_ATTR_RSSI, payload->RSSI_dbm_x100);
		packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, payload->RSSI_dbm_x100);
	}
	simSignalStrength = payload->RSSI_dbm_x100;
	simLQI = payload->LQI;
	simLastPacketTimestamp = payload->RX_timestamp_us-message_size*(RADIO_BYTE_AIR_TIME)+32;
	free(msg);
	return message_size;
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
	uint32_t ChannelIsFree;
	struct labscim_radio_response* resp;
	struct contiki_radio_cca crcca;
	uint32_t sequence_number;
	sequence_number = radio_command(gNodeOutputBuffer, CONTIKI_RADIO_PERFORM_CCA, (void*)&crcca, sizeof(struct contiki_radio_cca));
	//channel assessment is a blocking call that takes some time. omnet must go on
	protocol_yield(gNodeOutputBuffer);
	resp =  (struct labscim_radio_response*)socket_wait_for_command(LABSCIM_RADIO_RESPONSE, sequence_number);
	socket_process_all_commands();
	if(resp->radio_response_code == CONTIKI_RADIO_CCA_RESULT)
	{
		ChannelIsFree = ((struct contiki_radio_cca*)resp->radio_struct)->ChannelIsFree;
	}
	else
	{
		//something very wrong happened
		while(1);
	}
	free(resp);
	return ChannelIsFree;
}

/*---------------------------------------------------------------------------*/
static void
radio_setup(void)
{
	if(gRadioSetupPending)
	{
		uint32_t sequence_number;
		sequence_number = radio_command(gNodeOutputBuffer, CONTIKI_RADIO_SETUP, (void*)&gRadioParameters, sizeof(struct contiki_radio_setup));
		gRadioSetupPending = 0;
	}
	return;
}

/*---------------------------------------------------------------------------*/
static int
radio_send(const void *payload, unsigned short payload_len)
{
	int result;
	int radio_was_on = simRadioHWOn;

	if(payload_len > LABSCIM_RADIO_BUFSIZE) {
		return RADIO_TX_ERR;
	}
	if(payload_len == 0) {
		return RADIO_TX_ERR;
	}

	if(radio_was_on) {
		ENERGEST_SWITCH(ENERGEST_TYPE_LISTEN, ENERGEST_TYPE_TRANSMIT);
	}
	else
	{
		radio_tx_on();
		simRadioHWOn = 1;
		ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
	}

	/* Transmit on CCA */
	if(LABSCIM_TRANSMIT_ON_CCA && send_on_cca && !channel_clear())
	{
		result = RADIO_TX_COLLISION;
	}
	else
	{
		struct contiki_radio_payload* msg;
		struct labscim_protocol_header* resp;
		uint32_t sequence_number;
		msg = (struct contiki_radio_payload*)malloc(FIXED_SIZEOF_CONTIKI_RADIO_PAYLOAD + payload_len);
		if(msg==NULL)
		{
			perror("\nMalloc error \n");
			return RADIO_TX_ERR;
		}
		/* Copy packet data to temporary storage */
		memcpy(msg->Message, payload, payload_len);
		msg->MessageSize_bytes = payload_len;
		msg->LQI = 0;
		msg->RSSI_dbm_x100 = 0;
		msg->RX_timestamp_us = 0;
		sequence_number = radio_command(gNodeOutputBuffer, CONTIKI_RADIO_SEND, (uint8_t*)msg, FIXED_SIZEOF_CONTIKI_RADIO_PAYLOAD + payload_len);
		protocol_yield(gNodeOutputBuffer);
		do{
			resp =  (struct labscim_protocol_header*)socket_wait_for_command(0, 0);
			if(resp->request_sequence_number == sequence_number)
			{
				labscim_set_time(((struct labscim_radio_response*)resp)->current_time);
				//this is our answer
				break;
			}
			else
			{
				//this is something else, process and keep waiting
				//TODO: this behavior is wrong. right behavior: if this function is running from interrupt context, all other interrupt events
				//TODO: must be delayed, and non interrupt must wait until the end of interrupt context
				//TODO: but for now we just believe that this wont be happening
				socket_process_command(resp);
				protocol_yield(gNodeOutputBuffer);
			}
		}while(1); //ugly?
		result = RADIO_TX_OK;
		free(msg);
		free(resp);
	}
	if(radio_was_on)
	{
		ENERGEST_SWITCH(ENERGEST_TYPE_TRANSMIT, ENERGEST_TYPE_LISTEN);
	}
	else
	{
		ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
	}
	simRadioHWOn = radio_was_on;
	return result;
}
/*---------------------------------------------------------------------------*/
static int
prepare_packet(const void *data, unsigned short len)
{
	struct contiki_radio_payload* pkt;
	if(len > LABSCIM_RADIO_BUFSIZE) {
		return RADIO_TX_ERR;
	}
	pkt = (struct contiki_radio_payload*) malloc(FIXED_SIZEOF_CONTIKI_RADIO_PAYLOAD + len);
	if(pkt==NULL)
	{
		perror("\nMalloc error \n");
		return RADIO_TX_ERR;
	}
	pkt->MessageSize_bytes = len;
	memcpy(pkt->Message,data,len);
	labscim_ll_insert_at_back(&gOutboundPackets,(void*)pkt);
	return 0;
}
/*---------------------------------------------------------------------------*/
static int
transmit_packet(unsigned short len)
{
	int ret = RADIO_TX_ERR;
	struct contiki_radio_payload* pkt = (struct contiki_radio_payload*)labscim_ll_pop_front(&gOutboundPackets);
	if(pkt != NULL) {
		ret = radio_send(pkt->Message, pkt->MessageSize_bytes);
		free(pkt);
	}

	return ret;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
	//return simRadioState == RECEPTION_STATE_RECEIVING;
	struct contiki_radio_state state;
	uint32_t result;
	uint32_t sequence_number;
	struct labscim_radio_response* resp;
	state.State = 0;
	sequence_number = radio_command(gNodeOutputBuffer, CONTIKI_RADIO_GET_STATE,(void*)&state, sizeof(struct contiki_radio_state));
	resp =  (struct labscim_radio_response*)socket_wait_for_command(LABSCIM_RADIO_RESPONSE, sequence_number);
	if(resp->radio_response_code == CONTIKI_RADIO_STATE_RESULT)
	{
		result = ((struct contiki_radio_state*)resp->radio_struct)->State;
	}
	else
	{
		//something very wrong happened
		while(1);
	}
	free(resp);
	return result==RECEPTION_STATE_RECEIVING;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
	return labscim_ll_size(&gReceivedPackets)>0;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(labscim_radio_process, ev, data)
{
	int len;

	PROCESS_BEGIN();

	while(1) {
		PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
		if(poll_mode) {
			continue;
		}

		packetbuf_clear();
		len = radio_read(packetbuf_dataptr(), PACKETBUF_SIZE);
		if(len > 0) {
			packetbuf_set_datalen(len);
			NETSTACK_MAC.input();
		}
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static int
init(void)
{
	labscim_ll_init_list(&gReceivedPackets);
	labscim_ll_init_list(&gOutboundPackets);

	gRadioParameters.Bandwidth_Hz = DOT_15_4G_CHANNEL_SPACING*1000;
	gRadioParameters.Bitrate_bps = DOT_15_4G_SYMBOLRATE;
	gRadioParameters.Power_dbm = (uint32_t)tx_power_current->dbm;
	set_channel(IEEE802154_DEFAULT_CHANNEL);
	gRadioSetupPending = 1;
	radio_setup();

	process_start(&labscim_radio_process, NULL);
	return 1;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
	switch(param) {
	case RADIO_PARAM_RX_MODE:
		*value = 0;
		if(addr_filter) {
			*value |= RADIO_RX_MODE_ADDRESS_FILTER;
		}
		if(auto_ack) {
			*value |= RADIO_RX_MODE_AUTOACK;
		}
		if(poll_mode) {
			*value |= RADIO_RX_MODE_POLL_MODE;
		}
		return RADIO_RESULT_OK;
	case RADIO_PARAM_TX_MODE:
		*value = 0;
		if(send_on_cca) {
			*value |= RADIO_TX_MODE_SEND_ON_CCA;
		}
		return RADIO_RESULT_OK;
	case RADIO_PARAM_LAST_RSSI:
		*value = simSignalStrength/100;
		return RADIO_RESULT_OK;
	case RADIO_PARAM_LAST_LINK_QUALITY:
		*value = simLQI;
		return RADIO_RESULT_OK;
	case RADIO_PARAM_RSSI:
		/*TODO: check how to do it on omnet */
		*value = -90 + simRadioChannel - 11;
		return RADIO_RESULT_OK;
	case RADIO_CONST_MAX_PAYLOAD_LEN:
		*value = (radio_value_t)LABSCIM_RADIO_BUFSIZE;
		return RADIO_RESULT_OK;
	case RADIO_PARAM_CHANNEL:
		    *value = (radio_value_t)get_channel();
		    return RADIO_RESULT_OK;
	case RADIO_PARAM_TXPOWER:
		*value = get_tx_power();
		return RADIO_RESULT_OK;
	case RADIO_CONST_CHANNEL_MIN:
		*value = 0;
		return RADIO_RESULT_OK;
	case RADIO_CONST_CHANNEL_MAX:
		*value = DOT_15_4G_CHANNEL_MAX;
		return RADIO_RESULT_OK;
	case RADIO_CONST_TXPOWER_MIN:
		*value = TX_POWER_DRIVER[get_tx_power_array_last_element()].dbm;
		return RADIO_RESULT_OK;
	case RADIO_CONST_TXPOWER_MAX:
		*value = OUTPUT_POWER_MAX;
		return RADIO_RESULT_OK;
	default:
		return RADIO_RESULT_NOT_SUPPORTED;
	}
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
	switch(param) {
	case RADIO_PARAM_RX_MODE:
		if(value & ~(RADIO_RX_MODE_ADDRESS_FILTER |
				RADIO_RX_MODE_AUTOACK | RADIO_RX_MODE_POLL_MODE)) {
			return RADIO_RESULT_INVALID_VALUE;
		}

		/* Only disabling is acceptable for RADIO_RX_MODE_ADDRESS_FILTER */
		if ((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0) {
			return RADIO_RESULT_NOT_SUPPORTED;
		}
		set_frame_filtering((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0);

		/* Only disabling is acceptable for RADIO_RX_MODE_AUTOACK */
		if ((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0) {
			return RADIO_RESULT_NOT_SUPPORTED;
		}
		set_auto_ack((value & RADIO_RX_MODE_AUTOACK) != 0);

		set_poll_mode((value & RADIO_RX_MODE_POLL_MODE) != 0);
		return RADIO_RESULT_OK;
	case RADIO_PARAM_TX_MODE:
		if(value & ~(RADIO_TX_MODE_SEND_ON_CCA)) {
			return RADIO_RESULT_INVALID_VALUE;
		}
		set_send_on_cca((value & RADIO_TX_MODE_SEND_ON_CCA) != 0);
		return RADIO_RESULT_OK;
	case RADIO_PARAM_CHANNEL:
	    if(value < 0 || value > DOT_15_4G_CHANNEL_MAX) {
	      return RADIO_RESULT_INVALID_VALUE;
	    }
	    if(get_channel() == (uint8_t)value) {
	      /* We already have that very same channel configured.
	       * Nothing to do here. */
	      return RADIO_RESULT_OK;
	    }
	    set_channel((uint8_t)value);
	    radio_setup();
	    return RADIO_RESULT_OK;
	case RADIO_PARAM_TXPOWER:
		if(value < TX_POWER_DRIVER[get_tx_power_array_last_element()].dbm || value > OUTPUT_POWER_MAX) {
			return RADIO_RESULT_INVALID_VALUE;
		}
		set_tx_power(value);
		radio_setup();
		return RADIO_RESULT_OK;
	default:
		return RADIO_RESULT_NOT_SUPPORTED;
	}
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
	if(param == RADIO_PARAM_LAST_PACKET_TIMESTAMP) {
		if(size != sizeof(rtimer_clock_t) || !dest) {
			return RADIO_RESULT_INVALID_VALUE;
		}
		*(rtimer_clock_t *)dest = (rtimer_clock_t)simLastPacketTimestamp;
		return RADIO_RESULT_OK;
	}
	return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
	return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
const struct radio_driver labscim_sun_radio_driver =
{
		init,
		prepare_packet,
		transmit_packet,
		radio_send,
		radio_read,
		channel_clear,
		receiving_packet,
		pending_packet,
		radio_rx_on,
		radio_rx_off,
		get_value,
		set_value,
		get_object,
		set_object
};
/*---------------------------------------------------------------------------*/

