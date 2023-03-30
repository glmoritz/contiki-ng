#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "labscim_protocol.h"
#include "labscim_helper.h"
#include "labscim_contiking_setup.h"


#include "sys/log.h"
#include <math.h>
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

extern struct contiki_node_setup* gBootMessage;

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
static uint32_t rx_count = 0;

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
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
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

  si.latency = (clock_time() - lt->downstream_generation_time) / 1e6;
  si.hop_count = 64 - UIP_IP_BUF->ttl + 1;
  memcpy(&si.signature, sender_addr->u8 + 8, sizeof(uint64_t));
  aoi(sender_addr, data, &si);

  LabscimSignalEmitDouble(gRTTSignal, (clock_time() - lt->upstream_generation_time) / 1e6);
  LabscimSignalEmitChar(gPacketReceivedSignal, (char *)&si, sizeof(struct signal_info));

  LOG_INFO("Received response '%.*s' from ", datalen, (char *)data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char str[32];
  uip_ipaddr_t dest_ipaddr;
  static uint32_t tx_count;
  static uint32_t missed_tx_count;
  static uint32_t NodeJoined = 0;
  static struct  labscim_test lt;
  double next_message_wait_s = 0;
	
  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  gPacketReceivedSignal = LabscimSignalRegister("PacketReceived");
  LabscimSignalSubscribe(gPacketReceivedSignal);
  gAoIMax = LabscimSignalRegister("UpstreamAoIMax");
  gAoIMin = LabscimSignalRegister("UpstreamAoIMin");
  gAoIArea = LabscimSignalRegister("UpstreamAoIArea");
  gPacketGeneratedSignal = LabscimSignalRegister("UpstreamPacketGenerated");
  gPacketLatencySignal = LabscimSignalRegister("UpstreamPacketLatency");
  gPacketHopcountSignal = LabscimSignalRegister("UpstreamPacketHopcount");
  gNodeJoinSignal = LabscimSignalRegister("NodeJoin");
  gRTTSignal = LabscimSignalRegister("PacketRTT");

  next_message_wait_s = 4 + LabscimExponentialRandomVariable(gBootMessage->packet_generation_rate_s - 4.0);
  LOG_INFO("First message in %d milliseconds\n", (uint32_t)(next_message_wait_s * 1000));
  etimer_set(&periodic_timer, next_message_wait_s * CLOCK_SECOND);

  while (1)
  {
    PROCESS_WAIT_EVENT();
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    if (ev == PROCESS_EVENT_TIMER)
    {

      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr))
      {
        if (!NodeJoined)
        {
          NodeJoined = 1;
          save_local_address();
          LabscimSignalEmitDouble(gNodeJoinSignal, NodeJoined);
        }

        /* Print statistics every 10th TX */
        if (tx_count % 10 == 0)
        {
          LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
                   tx_count, rx_count, missed_tx_count);
        }

        /* Send to DAG root */

        LOG_INFO("Sending request %u to ", tx_count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");
        lt.upstream_generation_time = clock_time();
        lt.downstream_generation_time = 0;
        lt.request_number++;
        LabscimSignalEmitDouble(gPacketGeneratedSignal, (double)(lt.upstream_generation_time) / 1e6);
        simple_udp_sendto(&udp_conn, (void *)&lt, sizeof(lt), &dest_ipaddr);
        tx_count++;
      }
      else if (NodeJoined)
      {
        NodeJoined = 0;
        LabscimSignalEmitDouble(gNodeJoinSignal, NodeJoined);
      }
      else
      {
        LOG_INFO("Not reachable yet\n");
        if (tx_count > 0)
        {
          missed_tx_count++;
        }
      }

      /* Add some jitter */
      next_message_wait_s = 4 + LabscimExponentialRandomVariable(gBootMessage->packet_generation_rate_s - 4.0);
      LOG_INFO("Next message in %d milliseconds\n", (uint32_t)(next_message_wait_s * 1000));
      etimer_set(&periodic_timer, next_message_wait_s * CLOCK_SECOND);
    }
  }

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