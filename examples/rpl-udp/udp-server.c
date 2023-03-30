/*
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

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "labscim_helper.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "labscim_protocol.h"
#include "labscim_helper.h"


#include "sys/log.h"
#include <math.h>

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

clock_time_t gLastReceivedPacket=0;

uint64_t gPacketGeneratedSignal;
uint64_t gPacketLatencySignal;
uint64_t gRTTSignal;
uint64_t gPacketHopcountSignal;
uint64_t gAoIMax;
uint64_t gAoIMin;
uint64_t gAoIArea;
uint64_t gNodeJoinSignal;

uint64_t gPacketReceivedSignal;

extern uint8_t gIsCoordinator;

#define MAX_NODES (256)
uint64_t gLastRcvMsgGenerationTime[MAX_NODES];
uint64_t gLastRcvMsgReceptionTime[MAX_NODES];

struct labscim_test
{
	clock_time_t upstream_generation_time;
	clock_time_t downstream_generation_time;
	uint8_t request_number;
} __attribute__((packed));

struct signal_info
{
	uint64_t signature;
	uint32_t hop_count;
	double latency;
	double aoi_max;
	double aoi_min;
	double aoi_area;	
} __attribute__((packed));


uint64_t gSignature;
static struct simple_udp_connection udp_conn;
PROCESS(udp_server_process, "UDP server");

AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
	struct labscim_test *lt = (struct labscim_test *)data;
	struct signal_info si;

	LOG_INFO("Received request '%.*s' from ", datalen, (char *)data);
	LOG_INFO_6ADDR(sender_addr);
	LOG_INFO_("\n");

	si.latency = (clock_time() - lt->upstream_generation_time) / 1e6;
	si.hop_count = 64 - UIP_IP_BUF->ttl + 1;

	memcpy(&si.signature, sender_addr->u8 + 8, sizeof(uint64_t));

	aoi(sender_addr, data, &si);

	LabscimSignalEmitChar(gPacketReceivedSignal, (char *)&si, sizeof(struct signal_info));

#if 1  // WITH_SERVER_REPLY
	/* send back the same string to the client as an echo reply */
	LOG_INFO("Sending response.\n");
	lt->downstream_generation_time = clock_time();
	LabscimSignalEmitDouble(gPacketGeneratedSignal,(double)(lt->downstream_generation_time)/1e6);
	simple_udp_sendto(&udp_conn, (void*)lt, sizeof(struct labscim_test), sender_addr);
#endif /* WITH_SERVER_REPLY */
}

static void
save_local_address(void)
{
	int i;
	uint8_t state;
	for (i = 0; i < UIP_DS6_ADDR_NB; i++)
	{
		state = uip_ds6_if.addr_list[i].state;
		if (uip_ds6_if.addr_list[i].isused && (state == ADDR_PREFERRED))
		{
			memcpy(&gSignature, (uip_ds6_if.addr_list[i].ipaddr).u8 + 8, sizeof(uint64_t));
		}
	}
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  gPacketReceivedSignal = LabscimSignalRegister("PacketReceived");
  gPacketGeneratedSignal = LabscimSignalRegister("DownstreamPacketGenerated");
  gPacketLatencySignal = LabscimSignalRegister("DownstreamPacketLatency");
  gPacketHopcountSignal = LabscimSignalRegister("DownstreamPacketHopcount");
  gAoIMax = LabscimSignalRegister("DownstreamAoIMax");
  gAoIMin = LabscimSignalRegister("DownstreamAoIMin");
  gAoIArea = LabscimSignalRegister("DownstreamAoIArea");

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

void signal_arrived(struct labscim_signal* sig)
{
	if (sig->signal_id == gPacketReceivedSignal)
	{
		struct signal_info* si = (struct signal_info*)(sig->signal);		
		if (si->signature == gSignature)
		{
			LabscimSignalEmitDouble(gPacketLatencySignal, si->latency);			
			LabscimSignalEmitDouble(gPacketHopcountSignal, si->hop_count);
			LabscimSignalEmitDouble(gAoIMin, si->aoi_min);
			LabscimSignalEmitDouble(gAoIMax, si->aoi_max);
			LabscimSignalEmitDouble(gAoIArea, si->aoi_area);
		}
	}
	free(sig);
}

void aoi(const uip_ipaddr_t *sender_addr, const uint8_t *data, struct signal_info* si)
{
	struct labscim_test* lt = (struct labscim_test*)data;
	if( gLastRcvMsgReceptionTime[ sender_addr->u8[15] ] > 0)
	{
		float LastAoiMin = ((float)(gLastRcvMsgReceptionTime[sender_addr->u8[15]] - gLastRcvMsgGenerationTime[sender_addr->u8[15]]))/1e6;
		float AoIMin = (clock_time()-lt->upstream_generation_time)/1e6;
		float AoIMax = (clock_time()-gLastRcvMsgGenerationTime[ sender_addr->u8[15] ])/1e6;

		float AoIBase = ( (float)(clock_time()-gLastRcvMsgGenerationTime[ sender_addr->u8[15] ]) )/1e6;
		float AoIArea = AoIBase * ((AoIMax + LastAoiMin)/2);	

		si->aoi_max = AoIMax;
		si->aoi_min = AoIMin;
		si->aoi_area = AoIArea;		
	}
	else
	{
		si->aoi_max = NAN;
		si->aoi_min = NAN;
		si->aoi_area = NAN;		
	}
	gLastRcvMsgGenerationTime[ sender_addr->u8[15] ] = lt->upstream_generation_time;
	gLastRcvMsgReceptionTime[ sender_addr->u8[15] ] = clock_time();
}