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
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

/* USB serial takes space, free more space elsewhere */
#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 160

/*******************************************************/
/******************* Configure TSCH ********************/
/*******************************************************/

/* IEEE802.15.4 PANID */
#define IEEE802154_CONF_PANID 0x81a5

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
#define TSCH_CONF_AUTOSTART 0

/* 6TiSCH minimal schedule length.
 * Larger values result in less frequent active slots: reduces capacity and saves energy. */
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 3

#if WITH_SECURITY

/* Enable security */
#define LLSEC802154_CONF_ENABLED 1

#endif /* WITH_SECURITY */

/*******************************************************/
/************* Other system configuration **************/
/*******************************************************/

///* Logging */
//#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_NONE
//#define TSCH_LOG_CONF_PER_SLOT                     0

#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_4_4

#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO
#define TSCH_LOG_CONF_PER_SLOT                     1






//#define TSCH_DEBUG_INIT() tsch_log("init")
//#define TSCH_DEBUG_INTERRUPT() tsch_log("irq")
//#define TSCH_DEBUG_RX_EVENT() tsch_log("rx")
//#define TSCH_DEBUG_TX_EVENT() tsch_log("tx")
//#define TSCH_DEBUG_SLOT_START() tsch_log("slot start")
//#define TSCH_DEBUG_SLOT_END() tsch_log("slot end")

#define QUEUEBUF_CONF_NUM 32
#define NBR_TABLE_CONF_MAX_NEIGHBORS 100
#define NETSTACK_MAX_ROUTE_ENTRIES 100
#define UIP_CONF_UDP_CONNS 100

#define TSCH_CONF_ASSOCIATION_POLL_FREQUENCY 100

//since our timeslot is 40ms (from default 10ms at 2.4GHz), orchestra default periods were divided by 4 (the nearest prime)
#define ORCHESTRA_CONF_EBSF_PERIOD (89)
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD (17)
#define ORCHESTRA_CONF_UNICAST_PERIOD (11)

//#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE (uint8_t[]){ 16, 17, 23, 18, 26, 15, 25, 22, 19, 11, 12, 13, 24, 14, 20, 21 }
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE (uint8_t[]){ 6, 2,33,36,27, 0,12,14,13, 4,24, 5, 3,20,34,10, 8,11,16,31,19, 1,25,30,18, 7,21, 9,22,17,29,35,26,23,28,32,15 }

#define TSCH_CONF_JOIN_HOPPING_SEQUENCE (uint8_t[]){ 16, 22 }


#define TSCH_CONF_EB_PERIOD     (4 * CLOCK_SECOND)
#define TSCH_CONF_MAX_EB_PERIOD (4 * CLOCK_SECOND)


#endif /* PROJECT_CONF_H_ */
