/*
 * Copyright (c) 2015, SICS Swedish ICT.
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
 */
/**
 * \file
 *         A RPL+TSCH node able to act as either a simple node (6ln),
 *         DAG Root (6dr) or DAG Root with security (6dr-sec)
 *         Press use button at startup to configure.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki.h"

#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "net/ipv6/simple-udp.h"

#include "labscim_helper.h"

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

static struct simple_udp_connection udp_conn;
#define SEND_INTERVAL		  (15 * CLOCK_SECOND)

clock_time_t gLastReceivedPacket=0;

uint64_t gPacketGeneratedSignal;
uint64_t gPacketLatencySignal;
uint64_t gRTTSignal;
uint64_t gPacketHopcountSignal;
uint64_t gAoIMax;
uint64_t gAoIMin;
uint64_t gNodeJoinSignal;
uint64_t gNodeLeaveSignal;

extern uint8_t gIsCoordinator;



/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);


struct labscim_test
{
	clock_time_t upstream_generation_time;
	clock_time_t downstream_generation_time;
	uint8_t request_number;
} __attribute__((packed));


static void
udp_server_rx_callback(struct simple_udp_connection *c,
		const uip_ipaddr_t *sender_addr,
		uint16_t sender_port,
		const uip_ipaddr_t *receiver_addr,
		uint16_t receiver_port,
		const uint8_t *data,
		uint16_t datalen)
{

	struct labscim_test* lt = (struct labscim_test*)data;
	LOG_INFO("Received request '%.*s' from ", datalen, (char *) data);
	LOG_INFO_6ADDR(sender_addr);
	LOG_INFO_("\n");

	LabscimSignalEmit(gPacketLatencySignal,(clock_time()-lt->upstream_generation_time)/1e6);
	LabscimSignalEmit(gPacketHopcountSignal,64-UIP_IP_BUF->ttl+1);

#if WITH_SERVER_REPLY
	/* send back the same string to the client as an echo reply */
	LOG_INFO("Sending response.\n");
	lt->downstream_generation_time = clock_time();
	LabscimSignalEmit(gPacketGeneratedSignal,(double)(lt->downstream_generation_time)/1e6);
	simple_udp_sendto(&udp_conn, (void*)lt, sizeof(struct labscim_test), sender_addr);
#endif /* WITH_SERVER_REPLY */
}

static void
udp_client_rx_callback(struct simple_udp_connection *c,
		const uip_ipaddr_t *sender_addr,
		uint16_t sender_port,
		const uip_ipaddr_t *receiver_addr,
		uint16_t receiver_port,
		const uint8_t *data,
		uint16_t datalen)
{
	struct labscim_test* lt = (struct labscim_test*)data;
	LabscimSignalEmit(gPacketLatencySignal,(clock_time()-lt->downstream_generation_time)/1e6);
	LabscimSignalEmit(gRTTSignal,(clock_time()-lt->upstream_generation_time)/1e6);
	LabscimSignalEmit(gPacketHopcountSignal,64-UIP_IP_BUF->ttl+1);
	LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
	LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
	LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
	LOG_INFO_("\n");
}



clock_time_t gBootTime;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
	//int is_coordinator;
	static struct etimer periodic_timer;
	static unsigned count;
	static char str[32];
	static struct  labscim_test lt;
	static uint32_t NodeJoined = 0;
	static uip_ipaddr_t dest_ipaddr;




	lt.request_number = 0;

	PROCESS_BEGIN();

	//is_coordinator = 0;
	gBootTime = clock_time();

//#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_Z1 || CONTIKI_TARGET_LABSCIM
//	is_coordinator = (node_id == 1);
//#endif




	if(gIsCoordinator)
	{
		gPacketGeneratedSignal = LabscimSignalRegister("TSCHDownstreamPacketGenerated");
		gPacketLatencySignal = LabscimSignalRegister("TSCHUpstreamPacketLatency");
		gPacketHopcountSignal = LabscimSignalRegister("TSCHUpstreamPacketHopcount");
		gAoIMax = LabscimSignalRegister("TSCHUpstreamAoIMax");
		gAoIMin = LabscimSignalRegister("TSCHUpstreamAoIMin");



		NETSTACK_MAC.on();

		NETSTACK_ROUTING.root_start();

		/* Initialize UDP connection */
		simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
				UDP_CLIENT_PORT, udp_server_rx_callback);

	}
	else
	{
		gAoIMax = LabscimSignalRegister("TSCHDownstreamAoIMax");
		gAoIMin = LabscimSignalRegister("TSCHDownstreamAoIMin");
		gPacketGeneratedSignal = LabscimSignalRegister("TSCHUpstreamPacketGenerated");
		gPacketLatencySignal = LabscimSignalRegister("TSCHDownstreamPacketLatency");
		gPacketHopcountSignal = LabscimSignalRegister("TSCHDownstreamPacketHopcount");
		gNodeJoinSignal = LabscimSignalRegister("TSCHNodeJoin");
		gNodeLeaveSignal = LabscimSignalRegister("TSCHNodeLeave");
		gRTTSignal = LabscimSignalRegister("TSCHPacketRTT");

		NETSTACK_MAC.on();

		/* Initialize UDP connection */
		simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
				UDP_SERVER_PORT, udp_client_rx_callback);

		etimer_set(&periodic_timer, random_rand()%(SEND_INTERVAL/4) + SEND_INTERVAL);

		while(1) {
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

			if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
				if(!NodeJoined)
				{
					NodeJoined = 1;
					LabscimSignalEmit(gNodeJoinSignal,node_id);
				}
				/* Send to DAG root */
				LOG_INFO("Sending request %u to ", count);
				LOG_INFO_6ADDR(&dest_ipaddr);
				LOG_INFO_("\n");
				lt.upstream_generation_time = clock_time();
				lt.downstream_generation_time = 0;
				lt.request_number++;
				LabscimSignalEmit(gPacketGeneratedSignal,(double)(lt.upstream_generation_time)/1e6);
				snprintf(str, sizeof(str), "hello %d", count);
				simple_udp_sendto(&udp_conn, (void*)&lt, sizeof(lt), &dest_ipaddr);
				count++;
			}
			else if (NodeJoined && (!NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) )
			{
				NodeJoined = 0;
				LabscimSignalEmit(gNodeLeaveSignal,node_id);
			}
			else
			{
				LOG_INFO("Not reachable yet\n");
			}

			/* Add some jitter */
			etimer_set(&periodic_timer, SEND_INTERVAL
					- CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
		}


	}


#if WITH_PERIODIC_ROUTES_PRINT
	{
		static struct etimer et;
		/* Print out routing tables every minute */
		etimer_set(&et, CLOCK_SECOND * 60);
		while(1) {
			/* Used for non-regression testing */
#if (UIP_MAX_ROUTES != 0)
			PRINTF("Routing entries: %u\n", uip_ds6_route_num_routes());
#endif
#if (UIP_SR_LINK_NUM != 0)
			PRINTF("Routing links: %u\n", uip_sr_num_nodes());
#endif
			PROCESS_YIELD_UNTIL(etimer_expired(&et));
			etimer_reset(&et);
		}
	}
#endif /* WITH_PERIODIC_ROUTES_PRINT */

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
